#pragma once

/********** Instrument edit **********/
#include "./instrument/add_instrument_command.hpp"
#include "./instrument/remove_instrument_command.hpp"
#include "./instrument/change_instrument_name_command.hpp"
#include "./instrument/clone_instrument_command.hpp"
#include "./instrument/deep_clone_instrument_command.hpp"

/********** Pattern edit **********/
#include "./pattern/set_key_on_to_step_command.hpp"
#include "./pattern/set_key_off_to_step_command.hpp"
#include "./pattern/erase_step_command.hpp"
#include "./pattern/set_instrument_to_step_command.hpp"
#include "./pattern/erase_instrument_in_step_command.hpp"
#include "./pattern/set_volume_to_step_command.hpp"
#include "./pattern/erase_volume_in_step_command.hpp"
#include "./pattern/set_effect_id_to_step_command.hpp"
#include "./pattern/erase_effect_in_step_command.hpp"
#include "./pattern/set_effect_value_to_step_command.hpp"
#include "./pattern/erase_effect_value_in_step_command.hpp"
#include "./pattern/insert_step_command.hpp"
#include "./pattern/delete_previous_step_command.hpp"
#include "./pattern/paste_copied_data_to_pattern_command.hpp"
#include "./pattern/erase_cells_in_pattern_command.hpp"
#include "./pattern/paste_mix_copied_data_to_pattern_command.hpp"
#include "./pattern/increase_note_key_in_pattern_command.hpp"
#include "./pattern/decrease_note_key_in_pattern_command.hpp"
#include "./pattern/increase_note_octave_in_pattern_command.hpp"
#include "./pattern/decrease_note_octave_in_pattern_command.hpp"
#include "./pattern/expand_pattern_command.hpp"
#include "./pattern/shrink_pattern_command.hpp"
#include "./pattern/set_echo_buffer_access_command.hpp"

/********** Order edit **********/
#include "./order/set_pattern_to_order_command.hpp"
#include "./order/insert_order_below_command.hpp"
#include "./order/delete_order_command.hpp"
#include "./order/paste_copied_data_to_order_command.hpp"
#include "./order/duplicate_order_command.hpp"
#include "./order/move_order_command.hpp"
#include "./order/clone_patterns_command.hpp"
#include "./order/clone_order_command.hpp"
