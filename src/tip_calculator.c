#include <pebble.h>

//~CONSTANTS-----------------------------------------------------
//Constant indicating the number of rows in the menu
static const uint8_t NUM_ROWS = 4;
static const uint8_t NUM_AVAILABLE_DIGITS = 11;
//Used for logging
static const char * FILE_NAME = "tip_calculator.c";
//The amount of precision to use with floating points when converting to string
static const uint8_t FLOATING_POINT_PRECISION = 100;

//~VARIABLES-------------------------------------------------------------------------
//Main Window
//Required for all pebble apps, analogous to the layout xml file in Android
static Window *window;

//Layer which holds the 3 TextLayers below
//Allows for selecting etc.
static MenuLayer *menu_layer;

//The amount paid
static float paid_amount = 0;

//The percent to tip
static int16_t tip_percentage = -1;

//The amount to tip
static float tip_amount = -1;

//The total, including tip
static float total_amount = -1;

//The row that is currently being edited
//Can be either 0 or 1
//0 == paid amount
//1 == tip percentage
static uint8_t active_row = 0;

//Boolean value indicating whether we have entered into the 
//selected layer for editing
static bool selection_enabled = false;

//The current digit being edited in the current selection
static uint8_t cur_digit_index = 0;

//Array that can be iterated over to select digits for entry
static char available_digits[] = {'.', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

//The full number being edited in the current selection
static char full_number[11];

//The current index within full_number
static uint8_t full_number_index = 0;

//Boolean to indicate whether the full_number field has had a decimal_point used
static bool decimal_point_used = false;

//The number of digits that have come after placing a decimal point
static uint8_t digits_after_decimal = 0;

//~FUNCTIONS============================================================================================================================
//Takes a float, and a pre-allocated char * with the length of it and 
//converts the float to a c-string stored in the char * passed in.
//the parameter floating_point_precision should be >= 10, it indicates how many decimal points
//of precision are required.
//Each 0 in the number is one place of precision
static void simple_ftoa(double passed_float, char *my_string, uint16_t string_length, uint32_t floating_point_precision) {

    int32_t cast_float = passed_float;
    //Add 1.0 to identify if we ended up with leading 0s
    uint32_t decimals_of_float = (((passed_float + 1.0) - cast_float) + (0.5 / floating_point_precision)) * floating_point_precision;

    //Two zeros after decimal point
    if (decimals_of_float == 100) {

        snprintf(my_string, (string_length - 1), "%ld.00", cast_float);
    }
    //One zero immediately after decimal point
    else if ((decimals_of_float % 100) / 10 == 0) {

        decimals_of_float %= 100;
        snprintf(my_string, (string_length - 1), "%ld.0%ld", cast_float, decimals_of_float);
    }
    //No zeros after decimal point
    else {

        decimals_of_float %= 100;
        snprintf(my_string, (string_length - 1), "%ld.%ld", cast_float, decimals_of_float);
    }
}

//Takes a null-terminated C-string and converts it to a float
//Behavior is undefined if my_string does not fit a float format
static double simple_atof(const char *my_string) {

    double result = 0;

    uint8_t decimal_index = 0;
    int8_t negative = 1;

    //Account for negatives
    if (my_string[0] == '-') {
    
        negative = -1;
    }

    //Find decimal point
    for (; decimal_index < strlen(my_string) && my_string[decimal_index] != '.'; decimal_index++);
    if (decimal_index > 0) {
    
        decimal_index--;
    }
    //Account for negatives
    if (negative < 0) {
 
        decimal_index--;
    }

    //Set value of multiplier
    double multiplier = 1;
    uint8_t j = 0;
    for (; j < decimal_index; j++, multiplier *= 10);

    //Iterate over characters
    uint8_t i = 0;
    for (; i < strlen(my_string); i++, multiplier /= 10) {

        if (my_string[i] == '.' && i != 0) {
            
            multiplier *= 10;
        }
        else if (my_string[i] != '.') {

            result += ((my_string[i] - '0') * multiplier);
        }
    }

    return (result * negative);
}

//~MENU LAYER CALLBACKS-----------------------------------------------------------------------------------------------------------------
//Draws each row in the menu
static void menu_draw_row_callback(GContext *context, const Layer *cell_layer, MenuIndex *cell_index, void * callback_context) {

    switch (cell_index->row) {

        //Price Entry
        case 0:
            //If NOT in selection mode
            if (!selection_enabled || (selection_enabled && active_row != 0)) {
                
                //If no entry has been given yet
                if (paid_amount != 0) {
                    
                    char title_string[22];
                    char paid_amount_string[11];
                    simple_ftoa(paid_amount, paid_amount_string, 11, FLOATING_POINT_PRECISION);
                    snprintf(title_string, 21, "Paid: $%s", paid_amount_string);
                    app_log(33, FILE_NAME, 112, title_string);
                    menu_cell_title_draw(context, cell_layer, title_string);
                }
                //
                else {
                
                   menu_cell_title_draw(context, cell_layer, "Enter Subtotal:");
                }
            }
            //selection enabled on THIS ROW
            else if (selection_enabled && active_row == 0){  

                char title_string[13];
                snprintf(title_string, 12, "$%s%c", full_number, available_digits[cur_digit_index]);
                menu_cell_title_draw(context, cell_layer, title_string);
            }
            break;
        //Tip Percentage Entry
        case 1:
            if (!selection_enabled || (selection_enabled && active_row != 1)) {
             
                if (tip_percentage != -1) {
                    
                    char title_string[10];
                    snprintf(title_string, 9, "Tip: %d%%", tip_percentage);
                    menu_cell_title_draw(context, cell_layer, title_string);
                }
                else {
                
                   menu_cell_title_draw(context, cell_layer, "Tip %:");
                }   
            }
            else if (selection_enabled && active_row == 1) {

                char title_string[13];
                snprintf(title_string, 12, "%s%c%%", full_number, available_digits[cur_digit_index]);
                menu_cell_title_draw(context, cell_layer, title_string);   
            }
            break;
        //Tip Amount Display
        case 2:
            if (tip_amount == -1) {
                
                menu_cell_title_draw(context, cell_layer, "Tip Amount:");
            }
            else {

                char title_string[13];
                char tip_amount_string[11];
                simple_ftoa(tip_amount, tip_amount_string, 11, FLOATING_POINT_PRECISION);
                snprintf(title_string, 12, "$%s", tip_amount_string);
                menu_cell_title_draw(context, cell_layer, title_string);
            }
            break;
        case 3:
            if (total_amount == -1) {
                
                menu_cell_title_draw(context, cell_layer, "Total:");
            }
            else {

                char title_string[14];
                char total_amount_string[11];
                simple_ftoa(total_amount, total_amount_string, 11, FLOATING_POINT_PRECISION);
                snprintf(title_string, 13, "$%s", total_amount_string);
                menu_cell_title_draw(context, cell_layer, title_string);
            }
    }
}

//Callback returning the height of a cell
//Mine are all equally sized, so I just divide menu_layer's height by 3
static int16_t menu_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {

    GRect total_bounds = layer_get_bounds(menu_layer_get_layer(menu_layer));

    return total_bounds.size.h / NUM_ROWS;
}

//Callback returning the number of rows per section, based on the section
//4 for the first 1, 0 for all others
static uint16_t menu_get_number_of_rows_per_section(MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {

    return section_index > 0 ? 0 : NUM_ROWS;
}

//Callback returning the number of sections in the menu_layer
//Only have 1 section
static uint16_t menu_get_number_of_sections(MenuLayer *menu_layer, void *callback_context) {

    return 1;
}

//~Custom Callbacks to override menu/scroll layer ones-------------------------------------------------------------------
//Up Handler
static void menu_up_click_handler(ClickRecognizerRef recognizer, void *context) {

    //If we're editing within a layer
    if (selection_enabled) {

        //Increment cur_digit_index appropriately
        if (cur_digit_index + 1 == NUM_AVAILABLE_DIGITS) {

            //Allow decimal point
            if (active_row == 0)  {
                
                cur_digit_index = 0;
            }
            //Skip over decimal point
            else if (active_row == 1) {

                cur_digit_index = 1;
            }
        }
        else {

            cur_digit_index++;
        }

        //Reload data and re-draw!
        menu_layer_reload_data(menu_layer);
    }
    //If we're scolling
    else {

        if (active_row == 0) {
            return;
        }
    
        active_row--;
    
        MenuIndex menu_index;
        menu_index.row = active_row;
        const bool animated = true;
        menu_layer_set_selected_index(menu_layer, menu_index, MenuRowAlignNone, animated);
    }
}

//Down Handler
static void menu_down_click_handler(ClickRecognizerRef recognizer, void *context) {

    //If we're editing within a layer
    if (selection_enabled) {

        //Decrement cur_digit_index appropriately depending on which text field is getting filled in
        if ((active_row == 0 && cur_digit_index - 1 == -1) 
            || (active_row == 1 && cur_digit_index - 1 == 0)) {

            cur_digit_index = NUM_AVAILABLE_DIGITS - 1;
        }
        else {

            cur_digit_index--;
        }

        //Reload data and re-draw!
        menu_layer_reload_data(menu_layer);
    }
    //If we're scrolling
    else {

        if (active_row == (NUM_ROWS) - 1) {
            return;
        }
        
        active_row++;
        MenuIndex menu_index;
        menu_index.row = active_row;
        const bool animated = true;
        menu_layer_set_selected_index(menu_layer, menu_index, MenuRowAlignNone, animated);
    }
}

//Select Handler
static void menu_select_click_handler(ClickRecognizerRef recognizer, void *context) {

    //If we've entered a panel  
    if (selection_enabled) {

        //add case for long click on selection enabled

        //Handles text entry for paid amount
        if (active_row == 0) {

            //Check if decimal point used already
            if (decimal_point_used 
                && available_digits[cur_digit_index] == '.') {
                
                return;
            }

            //accept digit
            full_number[full_number_index] = available_digits[cur_digit_index];
            full_number_index++;

            //If we entered a decimal point, increment our count
            //No need for > 2 digits after the decimal point....
            if (decimal_point_used) {
                
                digits_after_decimal++;
            }

            //Check if we just used a decimal point
            if (available_digits[cur_digit_index] == '.') {

                decimal_point_used = true;
            }

            //Reset the digit to 0 AFTER flipping decimal_point_used
            cur_digit_index = 0;

            //Check if we have overstayed our digits
            if (full_number_index == 10 || digits_after_decimal == 2) {

                selection_enabled = false;
                full_number_index = 0;
                digits_after_decimal = 0;
                full_number[10] = '\0';
                decimal_point_used = false;

                paid_amount = simple_atof(full_number);

                //Whipe full_number
                for (int i = 0; i < 11; i++) {

                    full_number[i] = '\0';
                }

                //Calculate the values
                if (paid_amount != 0 && tip_percentage != -1) {
             
                    tip_amount = paid_amount * (tip_percentage / 100.0);
                    total_amount = paid_amount + tip_amount;
                }
            }
        }
        //Handles text entry for tip percentage
        else if (active_row == 1) {

            //accept digit
            full_number[full_number_index] = available_digits[cur_digit_index];
            full_number_index++;

            //Reset the digit to 0 AFTER flipping decimal_point_used
            cur_digit_index = 1;

            //Check if we have overstayed our digits
            if (full_number_index == 2) {

                selection_enabled = false;
                full_number_index = 0;
                full_number[10] = '\0';
                decimal_point_used = false;

                tip_percentage = atoi(full_number);

                //Whipe full_number
                for (int i = 0; i < 11; i++) {

                    full_number[i] = '\0';
                }

                //Calculate the values
                if (paid_amount != 0 && tip_percentage != -1) {
                    
                    tip_amount = paid_amount * (tip_percentage / 100.0);
                    total_amount = paid_amount + tip_amount;
                }
            }
        }
    }
    //
    else {

        switch (active_row) {

            case 0:
                //Switch to entry
                selection_enabled = true;
                cur_digit_index = 0;
                break;
            case 1:
                //Switch to entry mode
                selection_enabled = true;
                cur_digit_index = 1;
                break;
            case 2:
            case 3:
                //Calculate
                //tip_amount = paid_amount * (tip_percentage / 100);
                //total_amount = paid_amount + tip_amount;
                //redraw
                break;
        }
    }

    //Reload data and re-draw!
    menu_layer_reload_data(menu_layer);
}

//Setup callbacks for overriding MenuLayer default behavior
//Set initially
static void click_config_provider(void *context) {

    window_single_click_subscribe(BUTTON_ID_SELECT, menu_select_click_handler);
    window_single_click_subscribe(BUTTON_ID_UP, menu_up_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, menu_down_click_handler);
}

//~Required lifecycle functions-------------------------------------------------------------
//Loads the window, basically loads the entire gui of the app
static void window_load(Window *window) {

    //Base layer of the pebble app
    //Similar to a layout in Android
    //This particular layer is analogous to the base layout in the xml file in Android
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    //Define the MenuLayer, give it size and a starting point
    menu_layer = menu_layer_create(
        (GRect) {
            .origin = { 0, 0 },
            .size = { bounds.size.w, bounds.size.h } } );

    //Pass clicks through to menu_layer
    //menu_layer_set_click_config_onto_window(menu_layer, window);
    window_set_click_config_provider(window, click_config_provider);

    //Setup callbacks
    menu_layer_set_callbacks(menu_layer, NULL, 
        (MenuLayerCallbacks) {
            .draw_header = NULL, //No headers, not needed
            .draw_row = menu_draw_row_callback,
            .get_cell_height = menu_get_cell_height_callback,
            .get_header_height = NULL, //No headers, not needed
            .get_num_rows = menu_get_number_of_rows_per_section,
            .get_num_sections = menu_get_number_of_sections,
            .select_click = NULL, //Manually handling
            .select_long_click = NULL, //Manually handling
            .selection_changed = NULL
        });

    //Add the menu_layer to the base layer of the window
    layer_add_child(window_layer, menu_layer_get_layer(menu_layer));
}

//Leaking memory is bad, and if you exclude this function
//you will make your pebble feel bad
static void window_unload(Window *window) {

    menu_layer_destroy(menu_layer);
}

//Setup the application gui and the events that can or will
//occur while it runs
static void init(void) {

    window = window_create();
    window_set_window_handlers(window, 
        (WindowHandlers) {
            .load = window_load,
            .unload = window_unload,
    });
    const bool animated = true;
    window_stack_push(window, animated);
}

//Leaking memory is bad, and if you exclude this function
//you will make your pebble feel bad
static void deinit(void) {
    
    window_destroy(window);
}

//Main function
int main(void) {
    
    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();
    deinit();
}
