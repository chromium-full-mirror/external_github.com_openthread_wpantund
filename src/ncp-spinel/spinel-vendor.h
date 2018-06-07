/*
 *  Copyright (c) 2018, Nest Labs, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

enum {
    SPINEL_CMD_VENDOR_NEST_PROP_VALUE_SET = SPINEL_CMD_VENDOR__BEGIN,
    SPINEL_CMD_VENDOR_NEST_PROP_VALUE_GET,
    SPINEL_CMD_VENDOR_NEST_PROP_VALUE_IS,
};

enum {
    SPINEL_PROP_VENDOR_NEST_NETWORK_WAKE_CTRL = SPINEL_PROP_VENDOR__BEGIN,
    SPINEL_PROP_VENDOR_NEST_NETWORK_LURK_CTRL,
    SPINEL_PROP_VENDOR_NEST_NETWORK_WAKE_STATE,
    SPINEL_PROP_VENDOR_NEST_LEGACY_CREDENTIALS_RECOVERY,
    SPINEL_PROP_VENDOR_NEST_LEGACY_CREDENTIALS_ERASE,
    SPINEL_PROP_VENDOR_NEST_NETWORK_PARAMS,
    SPINEL_PROP_VENDOR_NEST_DEBUG,
    SPINEL_PROP_VENDOR_NEST_DATAPOLL_INTERVAL,
    SPINEL_PROP_VENDOR_NEST_PAW_DEST_ADDR,
};

enum
{
    SPINEL_VENDOR_NEST_NETWORK_WAKE_REASON_REMOTE       = 0,
    SPINEL_VENDOR_NEST_NETWORK_WAKE_REASON_HOST         = 1,
    SPINEL_VENDOR_NEST_NETWORK_WAKE_REASON_COMPLETE     = 2
};
