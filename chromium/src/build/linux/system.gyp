# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['sysroot!=""', {
        'pkg-config': '<(chroot_cmd) ./pkg-config-wrapper "<(sysroot)" "<(target_arch)" "<(system_libdir)"',
      }, {
        'pkg-config': 'pkg-config',
      }],
    ],

    # If any of the linux_link_FOO below are set to 1, then the corresponding
    # target will be linked against the FOO library (either dynamically or
    # statically, depending on the pkg-config files), as opposed to loading the
    # FOO library dynamically with dlopen.
    'linux_link_libgps%': 0,
    'linux_link_libpci%': 0,
    'linux_link_libspeechd%': 0,
    'linux_link_libbrlapi%': 0,

    # Used below for the various libraries. In this scope for sharing with GN.
    'libbrlapi_functions': [
      'brlapi_getHandleSize',
      'brlapi_error_location',
      'brlapi_strerror',
      'brlapi__acceptKeys',
      'brlapi__openConnection',
      'brlapi__closeConnection',
      'brlapi__getDisplaySize',
      'brlapi__enterTtyModeWithPath',
      'brlapi__leaveTtyMode',
      'brlapi__writeDots',
      'brlapi__readKey',
    ],
    'libgio_functions': [
      'g_settings_new',
      'g_settings_get_child',
      'g_settings_get_string',
      'g_settings_get_boolean',
      'g_settings_get_int',
      'g_settings_get_strv',
      'g_settings_list_schemas',
    ],
    'libpci_functions': [
      'pci_alloc',
      'pci_init',
      'pci_cleanup',
      'pci_scan_bus',
      'pci_fill_info',
      'pci_lookup_name',
    ],
    'libudev_functions': [
      'udev_device_get_action',
      'udev_device_get_devnode',
      'udev_device_get_parent',
      'udev_device_get_parent_with_subsystem_devtype',
      'udev_device_get_property_value',
      'udev_device_get_subsystem',
      'udev_device_get_sysattr_value',
      'udev_device_get_sysname',
      'udev_device_get_syspath',
      'udev_device_new_from_devnum',
      'udev_device_new_from_syspath',
      'udev_device_unref',
      'udev_enumerate_add_match_subsystem',
      'udev_enumerate_get_list_entry',
      'udev_enumerate_new',
      'udev_enumerate_scan_devices',
      'udev_enumerate_unref',
      'udev_list_entry_get_next',
      'udev_list_entry_get_name',
      'udev_monitor_enable_receiving',
      'udev_monitor_filter_add_match_subsystem_devtype',
      'udev_monitor_get_fd',
      'udev_monitor_new_from_netlink',
      'udev_monitor_receive_device',
      'udev_monitor_unref',
      'udev_new',
      'udev_set_log_fn',
      'udev_set_log_priority',
      'udev_unref',
    ],
  },
  'conditions': [
    [ 'chromeos==0 and use_ozone==0', {
      # Hide GTK and related dependencies for Chrome OS and Ozone, so they won't get
      # added back to Chrome OS and Ozone. Don't try to use GTK on Chrome OS and Ozone.
      'targets': [

      ],  # targets
    }],
    [ 'use_x11==1 or ozone_platform_ozonex==1', {
      # Hide X11 and related dependencies when use_x11=0
      'targets': [
        {
          'target_name': 'x11',
          'type': 'none',
          'toolsets': ['host', 'target'],
          'conditions': [
            ['_toolset=="target"', {
              'direct_dependent_settings': {
                'cflags': [
                  '<!@(<(pkg-config) --cflags x11)',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other x11 xi)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l x11 xi)',
                ],
              },
            }, {
              'direct_dependent_settings': {
                'cflags': [
                  '<!@(pkg-config --cflags x11)',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(pkg-config --libs-only-L --libs-only-other x11 xi)',
                ],
                'libraries': [
                  '<!@(pkg-config --libs-only-l x11 xi)',
                ],
              },
            }],
          ],
        },
        {
          'target_name': 'xcursor',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xcursor)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xcursor)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xcursor)',
            ],
          },
        },
        {
          'target_name': 'xcomposite',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xcomposite)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xcomposite)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xcomposite)',
            ],
          },
        },
        {
          'target_name': 'xdamage',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xdamage)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xdamage)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xdamage)',
            ],
          },
        },
        {
          'target_name': 'xext',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xext)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xext)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xext)',
            ],
          },
        },
        {
          'target_name': 'xfixes',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xfixes)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xfixes)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xfixes)',
            ],
          },
        },
        {
          'target_name': 'xi',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xi)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xi)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xi)',
            ],
          },
        },
        {
          'target_name': 'xrandr',
          'type': 'none',
          'toolsets': ['host', 'target'],
          'conditions': [
            ['_toolset=="target"', {
              'direct_dependent_settings': {
                'cflags': [
                  '<!@(<(pkg-config) --cflags xrandr)',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other xrandr)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l xrandr)',
                ],
              },
            }, {
              'direct_dependent_settings': {
                'cflags': [
                  '<!@(pkg-config --cflags xrandr)',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(pkg-config --libs-only-L --libs-only-other xrandr)',
                ],
                'libraries': [
                  '<!@(pkg-config --libs-only-l xrandr)',
                ],
              },
            }],
          ],
        },
        {
          'target_name': 'xrender',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xrender)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xrender)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xrender)',
            ],
          },
        },
        {
          'target_name': 'xtst',
          'type': 'none',
          'toolsets': ['host', 'target'],
          'conditions': [
            ['_toolset=="target"', {
              'direct_dependent_settings': {
                'cflags': [
                  '<!@(<(pkg-config) --cflags xtst)',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other xtst)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l xtst)',
                ],
              },
            }, {
              'direct_dependent_settings': {
                'cflags': [
                  '<!@(pkg-config --cflags xtst)',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(pkg-config --libs-only-L --libs-only-other xtst)',
                ],
                'libraries': [
                  '<!@(pkg-config --libs-only-l xtst)',
                ],
              },
            }]
          ]
        }
      ],  # targets
    }],
    ['use_x11==1 and chromeos==0', {
      'targets': [
        {
          'target_name': 'xscrnsaver',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xscrnsaver)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xscrnsaver)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xscrnsaver)',
            ],
          },
        },
      ],  # targets
    }],
    ['use_evdev_gestures==1', {
      'targets': [
        {
          'target_name': 'libevdev-cros',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags libevdev-cros)'
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other libevdev-cros)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l libevdev-cros)',
            ],
          },
        },
        {
          'target_name': 'libgestures',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags libgestures)'
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other libgestures)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l libgestures)',
            ],
          },
        },
      ],
    }],
    ['ozone_platform_gbm==1', {
      'targets': [
        {
          'target_name': 'gbm',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gbm)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gbm)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gbm)',
            ],
          },
        },
      ],
    }],
    ['ozone_platform_dri==1 or ozone_platform_gbm==1', {
      'targets': [
        {
          'target_name': 'libdrm',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags libdrm)',
            ],
          },
          'link_settings': {
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l libdrm)',
            ],
          },
        },
      ],
    }],
    ['use_udev==1', {
      'targets': [
        {
          'target_name': 'udev',
          'type': 'static_library',
          'conditions': [
            ['_toolset=="target"', {
              'include_dirs': [
                '../..',
              ],
              'hard_dependency': 1,
              'actions': [
                {
                  'variables': {
                    'output_h': '<(SHARED_INTERMEDIATE_DIR)/library_loaders/libudev0.h',
                    'output_cc': '<(INTERMEDIATE_DIR)/libudev0_loader.cc',
                    'generator': '../../tools/generate_library_loader/generate_library_loader.py',
                  },
                  'action_name': 'generate_libudev0_loader',
                  'inputs': [
                    '<(generator)',
                  ],
                  'outputs': [
                    '<(output_h)',
                    '<(output_cc)',
                  ],
                  'action': ['python',
                             '<(generator)',
                             '--name', 'LibUdev0Loader',
                             '--output-h', '<(output_h)',
                             '--output-cc', '<(output_cc)',
                             '--header', '"third_party/libudev/libudev0.h"',
                             '--link-directly=0',
                             '<@(libudev_functions)',
                  ],
                  'message': 'Generating libudev0 library loader',
                  'process_outputs_as_sources': 1,
                },
                {
                  'variables': {
                    'output_h': '<(SHARED_INTERMEDIATE_DIR)/library_loaders/libudev1.h',
                    'output_cc': '<(INTERMEDIATE_DIR)/libudev1_loader.cc',
                    'generator': '../../tools/generate_library_loader/generate_library_loader.py',
                  },
                  'action_name': 'generate_libudev1_loader',
                  'inputs': [
                    '<(generator)',
                  ],
                  'outputs': [
                    '<(output_h)',
                    '<(output_cc)',
                  ],
                  'action': ['python',
                             '<(generator)',
                             '--name', 'LibUdev1Loader',
                             '--output-h', '<(output_h)',
                             '--output-cc', '<(output_cc)',
                             '--header', '"third_party/libudev/libudev1.h"',
                             '--link-directly=0',
                             '<@(libudev_functions)',
                  ],
                  'message': 'Generating libudev1 library loader',
                  'process_outputs_as_sources': 1,
                },
              ],
            }],
          ],
        },
      ],
    }],
    ['use_libpci==1', {
      'targets': [
        {
          'target_name': 'libpci',
          'type': 'static_library',
          'cflags': [
            '<!@(<(pkg-config) --cflags libpci)',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
            'conditions': [
              ['linux_link_libpci==1', {
                'link_settings': {
                  'ldflags': [
                    '<!@(<(pkg-config) --libs-only-L --libs-only-other libpci)',
                  ],
                  'libraries': [
                    '<!@(<(pkg-config) --libs-only-l libpci)',
                  ],
                }
              }],
            ],
          },
          'include_dirs': [
            '../..',
          ],
          'hard_dependency': 1,
          'actions': [
            {
              'variables': {
                'output_h': '<(SHARED_INTERMEDIATE_DIR)/library_loaders/libpci.h',
                'output_cc': '<(INTERMEDIATE_DIR)/libpci_loader.cc',
                'generator': '../../tools/generate_library_loader/generate_library_loader.py',
              },
              'action_name': 'generate_libpci_loader',
              'inputs': [
                '<(generator)',
              ],
              'outputs': [
                '<(output_h)',
                '<(output_cc)',
              ],
              'action': ['python',
                         '<(generator)',
                         '--name', 'LibPciLoader',
                         '--output-h', '<(output_h)',
                         '--output-cc', '<(output_cc)',
                         '--header', '<pci/pci.h>',
                         # TODO(phajdan.jr): Report problem to pciutils project
                         # and get it fixed so that we don't need --use-extern-c.
                         '--use-extern-c',
                         '--link-directly=<(linux_link_libpci)',
                         '<@(libpci_functions)',
              ],
              'message': 'Generating libpci library loader',
              'process_outputs_as_sources': 1,
            },
          ],
        },
      ],
    }],
  ],  # conditions
  'targets': [
    {
      'target_name': 'dbus',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags dbus-1)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other dbus-1)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l dbus-1)',
        ],
      },
    },
    {
      'target_name': 'glib',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'variables': {
        'glib_packages': 'glib-2.0 gmodule-2.0 gobject-2.0 gthread-2.0',
      },
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags <(glib_packages))',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other <(glib_packages))',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l <(glib_packages))',
            ],
          },
        }, {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags <(glib_packages))',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other <(glib_packages))',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l <(glib_packages))',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'libcap',
      'type': 'none',
      'link_settings': {
        'libraries': [
          '-lcap',
        ],
      },
    },
    {
      'target_name': 'libresolv',
      'type': 'none',
      'link_settings': {
        'libraries': [
          '-lresolv',
        ],
      },
    },
    {
      'target_name': 'ssl',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'conditions': [
            ['use_openssl==1', {
              'dependencies': [
                '../../third_party/boringssl/boringssl.gyp:boringssl',
              ],
            }],
            ['use_openssl==0', {
              'dependencies': [
                '../../net/third_party/nss/ssl.gyp:libssl',
              ],
              'direct_dependent_settings': {
                'include_dirs+': [
                  # We need for our local copies of the libssl3 headers to come
                  # before other includes, as we are shadowing system headers.
                  '<(DEPTH)/net/third_party/nss/ssl',
                ],
                'cflags': [
                  '<!@(<(pkg-config) --cflags nss)',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other nss)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l nss | sed -e "s/-lssl3//")',
                ],
              },
            }],
            ['use_openssl==0 and clang==1', {
              'direct_dependent_settings': {
                'cflags': [
                  # There is a broken header guard in /usr/include/nss/secmod.h:
                  # https://bugzilla.mozilla.org/show_bug.cgi?id=884072
                  '-Wno-header-guard',
                ],
              },
            }],
          ]
        }],
      ],
    },
  ],
}
