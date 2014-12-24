# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'includes': [
    'build/common.gypi',
  ],
  'variables': {
    'webrtc_all_dependencies': [
      'base/base.gyp:*',
      'common.gyp:*',
      'libjingle/xmllite/xmllite.gyp:*',
      'libjingle/xmpp/xmpp.gyp:*',
      'p2p/p2p.gyp:*',
      'system_wrappers/source/system_wrappers.gyp:*',
    ],
  },
  'targets': [
    {
      'target_name': 'webrtc_all',
      'type': 'none',
      'dependencies': [
        '<@(webrtc_all_dependencies)',
        'webrtc',
      ],
    },
    {
      # TODO(pbos): This is intended to contain audio parts as well as soon as
      #             VoiceEngine moves to the same new API format.
      'target_name': 'webrtc',
      'type': 'static_library',
      'sources': [
        'call.h',
        'config.h',
        'experiments.h',
        'frame_callback.h',
        'transport.h',
      ],
      'dependencies': [
        'common.gyp:*',
      ],
    },
  ],
}
