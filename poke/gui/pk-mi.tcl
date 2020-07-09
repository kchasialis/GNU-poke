# pk-mi.tcl -- Control the inferior poke

# Copyright (C) 2020 Jose E. Marchesi

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see
# <http://www.gnu.org/licenses/>.

# Version of the poke MI protocol that we speak.
set poke_mi_version 0

set poke_version {}

set poke_confirmed_exit 0

proc pk_dispatch_msg_frame {frame_msg} {

    global poke_debug_mi_p
    global poke_mi_version

    global MI_MSG_TYPE_REQUEST
    global MI_MSG_TYPE_RESPONSE
    global MI_MSG_TYPE_EVENT
    global MI_EVENT_TYPE_INITIALIZED
    global MI_RESP_TYPE_EXIT

    set msg [::json::json2dict $frame_msg]

    if {$poke_debug_mi_p} {
        puts -nonewline "MI: recv: $frame_msg"
    }

    # Just ignore malformed packets.
    if {![dict exists $msg type]} {
        puts "error: ignoring malformed message from poke"
        return
    }
    set type [dict get $msg type]

    if {[expr $type == $MI_MSG_TYPE_EVENT]} {

        # Dispatch event.
        if {![dict exists $msg data type]} {
            puts "error: ignoring malformed event message from poke"
            return
        }
        set event_type [dict get $msg data type]

        if {[expr $event_type == $MI_EVENT_TYPE_INITIALIZED]} {

            global poke_version
            global poke_initialized_p

            if {![dict exists $msg data args version]
                || ![dict exists $msg data args mi_version]} {
                puts "error: ignoring malformed initialized event from poke"
                return
            }
            set version [dict get $msg data args version]
            set mi_version [dict get $msg data args mi_version]

            # Check that the MI version is the same dialect we speak
            if {[expr $mi_version != $poke_mi_version]} {
                pk_gui_fatal "Mismatch in the poke MI version"
            }

            set poke_version $version
            set poke_initialized_p 1

        } else {
            puts "error: ignoring unknown event message from poke"
        }

    } elseif {[expr $type == $MI_MSG_TYPE_RESPONSE]} {

        # Handle response.
        if {![dict exists $msg data type]} {
            puts "error: ignoring malformed response message from poke"
            return
        }
        set response_type [dict get $msg data type]

        if {[expr $response_type == $MI_RESP_TYPE_EXIT]} {

            global poke_confirmed_exit
            set poke_confirmed_exit 1
        }

    } elseif {[expr $type == $MI_MSG_TYPE_REQUEST]} {
        puts "info: ignoring request message from poke"
        return
    }
}


set poke_channel {}
set poke_pid {}

set poke_in_msg_size {}
set poke_in_msg {}

set poke_in_msg_size_bytes_read 0
set poke_in_msg_bytes_read 0

set poke_msg_size 0

proc pk_read_from_poke {} {
    global poke_channel
    global poke_in_msg_size
    global poke_in_msg
    global poke_in_msg_size_bytes_read
    global poke_in_msg_bytes_read
    global poke_msg_size

    if { [eof $poke_channel] } {
        catch {close $poke_channel}
        pk_gui_fatal "poke died!"
    }

    if { [expr $poke_in_msg_size_bytes_read < 4] } {

        set data [read $poke_channel \
                      [expr 4 - $poke_in_msg_size_bytes_read]]
        set nbytes [string length $data]

        set poke_in_msg_size \
            [string cat $poke_in_msg_size $data]
        incr poke_in_msg_size_bytes_read $nbytes

    } else {

        binary scan $poke_in_msg_size {c c c c} b3 b2 b1 b0
        set poke_msg_size \
            [expr ($b3 << 24) | ($b2 << 16) | ($b1 << 8) | $b0]

        if {[expr $poke_msg_size > 2048]} {
            # XXX protocol error
            puts "Protocol error!"
        }

        set data [read $poke_channel \
                      [expr $poke_msg_size - $poke_in_msg_bytes_read]]
        set nbytes [string length $data]

        set poke_in_msg \
            [string cat $poke_in_msg $data]
        incr poke_in_msg_bytes_read $nbytes
    }

    if {[expr $poke_in_msg_bytes_read != 0 \
             && $poke_in_msg_bytes_read == $poke_msg_size]} {

        global poke_confirmed_exit

        # Frame message is ready.  Process it.
        pk_dispatch_msg_frame ${poke_in_msg}
        if {$poke_confirmed_exit} {
            catch {close $poke_channel}
            return
        }

        # Prepare to receive another message.
        set poke_in_msg_size_bytes_read 0
        set poke_in_msg_bytes_read 0
        set poke_msg_size 0
        set poke_in_msg {}
        set poke_in_msg_size {}
    }
}

proc pk_send_frame_msg {payload} {

    global poke_channel

    set size [expr [string length $payload] + 1]
    set size_bytes [binary format {c c c c} \
                        [expr ($size >> 24) & 0xff] \
                        [expr ($size >> 16) & 0xff] \
                        [expr ($size >> 8) & 0xff] \
                        [expr ($size >> 0) & 0xff]]

    puts -nonewline $poke_channel $size_bytes
    puts $poke_channel $payload
    flush $poke_channel
}

proc pk_send_msg {msg} {

    global poke_debug_mi_p

    set json [pk_msg_to_json $msg]
    pk_send_frame_msg $json

    if {$poke_debug_mi_p} {
        puts "MI: sent: $json"
    }
}

set poke_initialized_p 0
set poke_version {}

proc pk_start_poke {} {

    global poke_channel
    global poke_pid
    global poke_initialized_p

    set poke_channel [open "|poke --mi" r+]
    set poke_pid [pid $poke_channel]

    fconfigure $poke_channel -buffering none -encoding binary \
        -blocking 0
    fileevent $poke_channel readable [list pk_read_from_poke]

    # Wait for the INITIALIZED event from poke.
    tkwait variable poke_initialized_p
}

set poke_confirmed_exit 0

proc pk_shutdown_poke {} {

    global poke_confirmed_exit
    global MI_REQ_TYPE_EXIT

    set msg [pk_msg_make_request $MI_REQ_TYPE_EXIT]
    pk_send_msg $msg

    # Wait for poke to confirm it can exit
    tkwait variable poke_confirmed_exit
}
