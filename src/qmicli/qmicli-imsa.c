/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * qmicli -- Command line interface to control QMI devices
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2023 Dylan Van Assche <me@dylanvanassche.be>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <assert.h>

#include <glib.h>
#include <gio/gio.h>

#include <libqmi-glib.h>

#include "qmicli.h"
#include "qmicli-helpers.h"

#if defined HAVE_QMI_SERVICE_IMSA

/* Context */
typedef struct {
    QmiDevice *device;
    QmiClientImsa *client;
    GCancellable *cancellable;
} Context;
static Context *ctx;

/* Options */
static gboolean get_ims_registration_status_flag;
static gboolean get_ims_services_status_flag;
static gboolean noop_flag;

static GOptionEntry entries[] = {
#if defined HAVE_QMI_MESSAGE_IMSA_GET_IMS_REGISTRATION_STATUS
    { "imsa-get-ims-registration-status", 0, 0, G_OPTION_ARG_NONE, &get_ims_registration_status_flag,
      "Get IMS registration status",
      NULL
    },
#endif
#if defined HAVE_QMI_MESSAGE_IMSA_GET_IMS_SERVICES_STATUS
    { "imsa-get-ims-services-status", 0, 0, G_OPTION_ARG_NONE, &get_ims_services_status_flag,
      "Get IMS services status",
      NULL
    },
#endif
    { "imsa-noop", 0, 0, G_OPTION_ARG_NONE, &noop_flag,
      "Just allocate or release a IMSA client. Use with `--client-no-release-cid' and/or `--client-cid'",
      NULL
    },
    { NULL }
};

GOptionGroup *
qmicli_imsa_get_option_group (void)
{
    GOptionGroup *group;

    group = g_option_group_new ("imsa",
                                "IMSA options:",
                                "Show IP Multimedia Subsystem Application Service options",
                                NULL,
                                NULL);
    g_option_group_add_entries (group, entries);

    return group;
}

gboolean
qmicli_imsa_options_enabled (void)
{
    static guint n_actions = 0;
    static gboolean checked = FALSE;

    if (checked)
        return !!n_actions;

    n_actions = (!!get_ims_registration_status_flag +
		 get_ims_services_status_flag +
                 noop_flag);

    if (n_actions > 1) {
        g_printerr ("error: too many IMSA actions requested\n");
        exit (EXIT_FAILURE);
    }

    checked = TRUE;
    return !!n_actions;
}

static void
context_free (Context *context)
{
    if (!context)
        return;

    if (context->cancellable)
        g_object_unref (context->cancellable);
    if (context->device)
        g_object_unref (context->device);
    if (context->client)
        g_object_unref (context->client);
    g_slice_free (Context, context);
}

static void
operation_shutdown (gboolean operation_status)
{
    /* Cleanup context and finish async operation */
    context_free (ctx);
    qmicli_async_operation_done (operation_status, FALSE);
}

#if defined HAVE_QMI_MESSAGE_IMSA_GET_IMS_REGISTRATION_STATUS

static void
get_ims_registration_status_ready (QmiClientImsa *client,
                                   GAsyncResult *res)
{
    QmiMessageImsaGetImsRegistrationStatusOutput *output;
    QmiImsaImsRegistrationStatus registration_status;
    QmiImsaRegistrationTechnology registration_technology;
    GError *error = NULL;

    output = qmi_client_imsa_get_ims_registration_status_finish (client, res, &error);
    if (!output) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        operation_shutdown (FALSE);
        return;
    }

    if (!qmi_message_imsa_get_ims_registration_status_output_get_result (output, &error)) {
        g_printerr ("error: couldn't get IMS registration status: %s\n", error->message);
        g_error_free (error);
        qmi_message_imsa_get_ims_registration_status_output_unref (output);
        operation_shutdown (FALSE);
        return;
    }

    g_print ("[%s] IMS registration:\n", qmi_device_get_path_display (ctx->device));

    if (qmi_message_imsa_get_ims_registration_status_output_get_ims_registration_status (output, &registration_status, NULL))
        g_print ("\t    Status: '%s'\n", qmi_imsa_ims_registration_status_get_string (registration_status));

    if (qmi_message_imsa_get_ims_registration_status_output_get_ims_registration_technology (output, &registration_technology, NULL))
    	g_print ("\tTechnology: '%s'\n", qmi_imsa_registration_technology_get_string (registration_technology));

    qmi_message_imsa_get_ims_registration_status_output_unref (output);
    operation_shutdown (TRUE);
}

#endif /* HAVE_QMI_MESSAGE_IMSA_GET_IMS_REGISTRATION_STATUS */

#if defined HAVE_QMI_MESSAGE_IMSA_GET_IMS_SERVICES_STATUS

static void
get_ims_services_status_ready (QmiClientImsa *client,
                               GAsyncResult *res)
{
    QmiMessageImsaGetImsServicesStatusOutput *output;
    QmiImsaServiceStatus service_sms_status;
    QmiImsaRegistrationTechnology service_sms_technology;
    QmiImsaServiceStatus service_voice_status;
    QmiImsaRegistrationTechnology service_voice_technology;
    QmiImsaServiceStatus service_vt_status;
    QmiImsaRegistrationTechnology service_vt_technology;
    QmiImsaServiceStatus service_ut_status;
    QmiImsaRegistrationTechnology service_ut_technology;
    QmiImsaServiceStatus service_vs_status;
    QmiImsaRegistrationTechnology service_vs_technology;
    GError *error = NULL;

    output = qmi_client_imsa_get_ims_services_status_finish (client, res, &error);
    if (!output) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        operation_shutdown (FALSE);
        return;
    }

    if (!qmi_message_imsa_get_ims_services_status_output_get_result (output, &error)) {
        g_printerr ("error: couldn't get IMS services status: %s\n", error->message);
        g_error_free (error);
        qmi_message_imsa_get_ims_services_status_output_unref (output);
        operation_shutdown (FALSE);
        return;
    }

    g_print ("[%s] IMS services:\n", qmi_device_get_path_display (ctx->device));

    g_print ("\tSMS service\n");

    if (qmi_message_imsa_get_ims_services_status_output_get_ims_sms_service_status (output, &service_sms_status, NULL))
       g_print ("\t\t    Status: '%s'\n", qmi_imsa_service_status_get_string (service_sms_status));

    if (qmi_message_imsa_get_ims_services_status_output_get_ims_sms_service_registration_technology (output, &service_sms_technology, NULL))
       g_print ("\t\tTechnology: '%s'\n", qmi_imsa_registration_technology_get_string (service_sms_technology));

    g_print ("\tVoice service\n");

    if (qmi_message_imsa_get_ims_services_status_output_get_ims_voice_service_status (output, &service_voice_status, NULL))
        g_print ("\t\t    Status: '%s'\n", qmi_imsa_service_status_get_string (service_voice_status));

    if (qmi_message_imsa_get_ims_services_status_output_get_ims_voice_service_registration_technology (output, &service_voice_technology, NULL))
        g_print ("\t\tTechnology: '%s'\n", qmi_imsa_registration_technology_get_string (service_voice_technology));

    g_print ("\tVideo Telephony service\n");

    if (qmi_message_imsa_get_ims_services_status_output_get_ims_video_telephony_service_status (output, &service_vt_status, NULL))
        g_print ("\t\t    Status: '%s'\n", qmi_imsa_service_status_get_string (service_vt_status));

    if (qmi_message_imsa_get_ims_services_status_output_get_ims_video_telephony_service_registration_technology (output, &service_vt_technology, NULL))
        g_print ("\t\tTechnology: '%s'\n", qmi_imsa_registration_technology_get_string (service_vt_technology));
       
    g_print ("\tUE to TAS service\n");

    if (qmi_message_imsa_get_ims_services_status_output_get_ims_ue_to_tas_service_status (output, &service_ut_status, NULL))
        g_print ("\t\t    Status: '%s'\n", qmi_imsa_service_status_get_string (service_ut_status));

    if (qmi_message_imsa_get_ims_services_status_output_get_ims_ue_to_tas_service_registration_technology (output, &service_ut_technology, NULL))
        g_print ("\t\tTechnology: '%s'\n", qmi_imsa_registration_technology_get_string (service_ut_technology));

    g_print ("\tVideo Share service\n");

    if (qmi_message_imsa_get_ims_services_status_output_get_ims_video_share_service_status (output, &service_vs_status, NULL))
        g_print ("\t\t    Status: '%s'\n", qmi_imsa_service_status_get_string (service_vs_status));

    if (qmi_message_imsa_get_ims_services_status_output_get_ims_video_share_service_registration_technology (output, &service_vs_technology, NULL))
        g_print ("\t\tTechnology: '%s'\n", qmi_imsa_registration_technology_get_string (service_vs_technology));

    qmi_message_imsa_get_ims_services_status_output_unref (output);
    operation_shutdown (TRUE);
}

#endif /* HAVE_QMI_MESSAGE_IMSA_GET_IMS_SERVICES_STATUS */

static gboolean
noop_cb (gpointer unused)
{
    operation_shutdown (TRUE);
    return FALSE;
}

void
qmicli_imsa_run (QmiDevice *device,
                 QmiClientImsa *client,
                 GCancellable *cancellable)
{
    /* Initialize context */
    ctx = g_slice_new (Context);
    ctx->device = g_object_ref (device);
    ctx->client = g_object_ref (client);
    ctx->cancellable = g_object_ref (cancellable);

#if defined HAVE_QMI_MESSAGE_IMSA_GET_IMS_REGISTRATION_STATUS
    if (get_ims_registration_status_flag) {
        g_debug ("Asynchronously getting IMS registration status...");

        qmi_client_imsa_get_ims_registration_status (ctx->client,
                                                     NULL,
                                                     10,
                                                     ctx->cancellable,
                                                     (GAsyncReadyCallback)get_ims_registration_status_ready,
                                                     NULL);
        return;
    }
#endif /* HAVE_QMI_MESSAGE_IMSA_GET_IMS_REGISTRATION_STATUS */
#if defined HAVE_QMI_MESSAGE_IMSA_GET_IMS_SERVICES_STATUS
    if (get_ims_services_status_flag) {
        g_debug ("Asynchronously getting IMS services status...");

        qmi_client_imsa_get_ims_services_status (ctx->client,
                                                 NULL,
                                                 10,
                                                 ctx->cancellable,
                                                 (GAsyncReadyCallback)get_ims_services_status_ready,
                                                 NULL);
        return;
    }
#endif /* HAVE_QMI_MESSAGE_IMSA_GET_IMS_SERVICES_STATUS */

    /* Just client allocate/release? */
    if (noop_flag) {
        g_idle_add (noop_cb, NULL);
        return;
    }

    g_warn_if_reached ();
}

#endif /* HAVE_QMI_SERVICE_IMSA */

