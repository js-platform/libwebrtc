#
# libjingle
# Copyright 2012, Google Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
  'includes': ['build/common.gypi'],
  'conditions': [
    ['os_posix == 1 and OS != "mac" and OS != "ios"', {
     'conditions': [
       ['sysroot!=""', {
         'variables': {
           'pkg-config': '../../../build/linux/pkg-config-wrapper "<(sysroot)" "<(target_arch)"',
         },
       }, {
         'variables': {
           'pkg-config': 'pkg-config'
         },
       }],
     ],
    }],
  ],
  'targets': [
    {
      'target_name': 'libjingle',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/third_party/expat/expat.gyp:expat',
        '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
        '<(webrtc_root)/base/base.gyp:rtc_base',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/third_party/expat/expat.gyp:expat',
        '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
      ],
    },  # target libjingle
    {
      'target_name': 'libjingle_media',
      'type': 'static_library',
      'include_dirs': [
        # TODO(jiayl): move this into the direct_dependent_settings of
        # usrsctp.gyp.
        '<(DEPTH)/third_party/usrsctp',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/usrsctp/usrsctp.gyp:usrsctplib',
        '<(webrtc_root)/webrtc.gyp:webrtc',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers_default',
        '<(webrtc_root)/libjingle/xmllite/xmllite.gyp:rtc_xmllite',
        '<(webrtc_root)/libjingle/xmpp/xmpp.gyp:rtc_xmpp',
        '<(webrtc_root)/p2p/p2p.gyp:rtc_p2p',
        'libjingle',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
        ],
      },
      'sources': [
        'media/base/codec.cc',
        'media/base/codec.h',
        'media/base/constants.cc',
        'media/base/constants.h',
        'media/base/cpuid.cc',
        'media/base/cpuid.h',
        'media/base/cryptoparams.h',
        'media/base/rtpdataengine.cc',
        'media/base/rtpdataengine.h',
        'media/base/rtpdump.cc',
        'media/base/rtpdump.h',
        'media/base/rtputils.cc',
        'media/base/rtputils.h',
        'media/base/streamparams.cc',
        'media/base/streamparams.h',
        'media/sctp/sctpdataengine.cc',
        'media/sctp/sctpdataengine.h',
        'media/webrtc/webrtccommon.h',
        'media/webrtc/webrtcexport.h',
        'media/webrtc/webrtcmediaengine.cc',
        'media/webrtc/webrtcmediaengine.h',
        'media/webrtc/webrtcmediaengine.cc',
      ],
      'conditions': [
        ['build_with_chromium==1', {
	  'dependencies': [
	  ],
	}, {
	  'dependencies': [
	  ],
	}],
        ['OS=="linux"', {
          'sources': [
          ],
          'include_dirs': [
          ],
          'cflags': [
          ],
          'libraries': [
          ],
        }],
        ['OS=="win"', {
          'sources': [
          ],
          'msvs_settings': {
            'VCLibrarianTool': {
              'AdditionalDependencies': [
                'd3d9.lib',
                'gdi32.lib',
                'strmiids.lib',
                'winmm.lib',
              ],
            },
          },
        }],
        ['OS=="mac"', {
          'sources': [
          ],
          'conditions': [
            ['target_arch=="ia32"', {
              'sources': [
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework Carbon',
                  ],
                },
              },
            }],
          ],
          'xcode_settings': {
            'WARNING_CFLAGS': [
              # TODO(ronghuawu): Update macdevicemanager.cc to stop using
              # deprecated functions and remove this flag.
              '-Wno-deprecated-declarations',
            ],
          },
          'link_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-weak_framework AVFoundation',
                '-framework Cocoa',
                '-framework CoreAudio',
                '-framework CoreVideo',
                '-framework OpenGL',
                '-framework QTKit',
              ],
            },
          },
        }],
        ['OS=="ios"', {
          'sources': [
            'media/devices/mobiledevicemanager.cc',
          ],
          'include_dirs': [
            # TODO(sjlee) Remove when vp8 is building for iOS.  vp8 pulls in
            # libjpeg which pulls in libyuv which currently disabled.
            '../third_party/libyuv/include',
          ],
        }],
        ['OS=="android"', {
          'sources': [
            'media/devices/mobiledevicemanager.cc',
          ],
        }],
      ],
    },  # target libjingle_media
    {
      'target_name': 'libjingle_p2p',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/third_party/libsrtp/libsrtp.gyp:libsrtp',
        'libjingle',
        'libjingle_media',
      ],
      'include_dirs': [
      ],
      'direct_dependent_settings': {
        'include_dirs': [
        ],
      },
      'sources': [
        'session/media/bundlefilter.cc',
        'session/media/bundlefilter.h',
        'session/media/channel.cc',
        'session/media/channel.h',
        'session/media/channelmanager.cc',
        'session/media/channelmanager.h',
        'session/media/mediamonitor.cc',
        'session/media/mediamonitor.h',
        'session/media/mediasession.cc',
        'session/media/mediasession.h',
        'session/media/rtcpmuxfilter.cc',
        'session/media/rtcpmuxfilter.h',
        'session/media/srtpfilter.cc',
        'session/media/srtpfilter.h',
      ],
    },  # target libjingle_p2p
    {
      'target_name': 'libjingle_peerconnection',
      'type': 'static_library',
      'dependencies': [
        'libjingle',
        'libjingle_media',
        'libjingle_p2p',
      ],
      'sources': [
        'app/webrtc/datachannel.cc',
        'app/webrtc/datachannel.h',
        'app/webrtc/datachannelinterface.h',
        'app/webrtc/fakeportallocatorfactory.h',
        'app/webrtc/jsep.h',
        'app/webrtc/jsepicecandidate.cc',
        'app/webrtc/jsepicecandidate.h',
        'app/webrtc/jsepsessiondescription.cc',
        'app/webrtc/jsepsessiondescription.h',
        'app/webrtc/mediaconstraintsinterface.cc',
        'app/webrtc/mediaconstraintsinterface.h',
        'app/webrtc/mediastreamsignaling.cc',
        'app/webrtc/mediastreamsignaling.h',
        'app/webrtc/notifier.h',
        'app/webrtc/peerconnection.cc',
        'app/webrtc/peerconnection.h',
        'app/webrtc/peerconnectionfactory.cc',
        'app/webrtc/peerconnectionfactory.h',
        'app/webrtc/peerconnectioninterface.h',
        'app/webrtc/peerconnectionproxy.h',
        'app/webrtc/portallocatorfactory.cc',
        'app/webrtc/portallocatorfactory.h',
        'app/webrtc/proxy.h',
        'app/webrtc/sctputils.cc',
        'app/webrtc/sctputils.h',
        'app/webrtc/statscollector.cc',
        'app/webrtc/statscollector.h',
        'app/webrtc/statstypes.cc',
        'app/webrtc/statstypes.h',
        'app/webrtc/webrtcsdp.cc',
        'app/webrtc/webrtcsdp.h',
        'app/webrtc/webrtcsession.cc',
        'app/webrtc/webrtcsession.h',
        'app/webrtc/webrtcsessiondescriptionfactory.cc',
        'app/webrtc/webrtcsessiondescriptionfactory.h',
      ],
    },  # target libjingle_peerconnection
  ],
}
