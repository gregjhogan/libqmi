# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2019 - 2021 Iñigo Martinez <inigomartinez@gmail.com>

test_unit = 'test-helpers'

exe = executable(
  test_unit,
  sources: test_unit + '.c',
  include_directories: top_inc,
  dependencies: libhelpers_dep,
)

test_env += {
  'G_TEST_BUILDDIR': meson.current_build_dir(),
  'G_TEST_SRCDIR': meson.current_source_dir(),
}

test(
  test_unit,
  exe,
  env: test_env,
)
