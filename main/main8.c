/* THIS IS THE FILE FOR AN 8 BIT LSTM */

#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/task.h"

#include <math.h>
#define PI 3.141592654
#include "parameters.h"

void lstmCellSimple(float input, const int * input_weights, const int * hidden_weights,
                       const int * bias, int * hidden_layer, int * cell_states);

int dense_nn(const int * input, const int * Weight, int bias);

int sigmoid_function (int input);

int float_to_int(float weight32);

float int_to_float(int weight8);

void app_main(void)
{
	printf("Hello world!\n");

	/* Print chip information */
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
			CONFIG_IDF_TARGET,
			chip_info.cores,
			(chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
			(chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

	printf("silicon revision %d, ", chip_info.revision);

	printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
			(chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

	printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    const unsigned MEASUREMENTS = 5000;
    uint64_t start = esp_timer_get_time();

    for (int retries = 0; retries < MEASUREMENTS; retries++) {

    	float input_value = 0.440561;
		float output_value;
		lstmCellSimple(input_value, lstm_cell_input_weights, lstm_cell_hidden_weights,
									 lstm_cell_bias, lstm_cell_hidden_layer, lstm_cell_cell_states);
		output_value = dense_nn(lstm_cell_hidden_layer, dense_weights, dense_bias);

    }

    uint64_t end = esp_timer_get_time();

    printf("%u iterations took %llu milliseconds (%llu microseconds per invocation)\n",
           MEASUREMENTS, (end - start)/1000, (end - start)/MEASUREMENTS);

	float input_value = 0.440561;
	//X_test=0.440561, y_test=0.460530
	//X_test=0.460530, y_test=0.450901
	//X_test=0.450901, y_test=0.418163
	//X_test=0.418163, y_test=0.480694
	//X_test=0.480694, y_test=0.456578
	//X_test=0.456578, y_test=0.441573
	float output_value;

	printf("%d\n", lstm_cell_hidden_layer[0]);
	//printf("%f\n", int_to_float(255));
	//printf("%f\n", quantization8_inverse(lstm_cell_hidden_layer)[0]);

	lstmCellSimple(input_value, lstm_cell_input_weights, lstm_cell_hidden_weights,
								 lstm_cell_bias, lstm_cell_hidden_layer, lstm_cell_cell_states);

	printf("%d\n", lstm_cell_hidden_layer[0]);

	output_value = int_to_float(dense_nn(lstm_cell_hidden_layer, dense_weights, dense_bias));

	printf("Output Value %f\n", output_value);


	for (int i = 10; i >= 0; i--) {
		printf("Restarting in %d seconds...\n", i);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	printf("Restarting now.\n");
	fflush(stdout);
	esp_restart();

}


void lstmCellSimple(float input, const int * input_weights, const int * hidden_weights,
                       const int * bias, int * hidden_layer, int * cell_states) {


    int new_hidden_layer[HUNIT];
    int new_cell_states[HUNIT];

    int input_gate[HUNIT];
    int forget_gate[HUNIT];
    int cell_candidate[HUNIT];
    int output_gate[HUNIT];

    for (int i = 0; i < HUNIT; ++i) {
        input_gate[i] = float_to_int(int_to_float(input_weights[0 * HUNIT + i]) * int_to_float(input));
        forget_gate[i] = float_to_int(int_to_float(input_weights[1 * HUNIT + i]) * int_to_float(input));
        cell_candidate[i] = float_to_int(int_to_float(input_weights[2 * HUNIT + i]) * int_to_float(input));
        output_gate[i] = float_to_int(int_to_float(input_weights[3 * HUNIT + i]) * int_to_float(input));

        for (int j = 0; j < HUNIT; ++j) {
            input_gate[i] += float_to_int(int_to_float(hidden_weights[(0 * HUNIT + i) * HUNIT + j]) * int_to_float(hidden_layer[j]));
            forget_gate[i] += float_to_int(int_to_float(hidden_weights[(1 * HUNIT + i) * HUNIT + j]) * int_to_float(hidden_layer[j]));
            cell_candidate[i] += float_to_int(int_to_float(hidden_weights[(2 * HUNIT + i) * HUNIT + j]) * int_to_float(hidden_layer[j]));
            output_gate[i] += float_to_int(int_to_float(hidden_weights[(3 * HUNIT + i) * HUNIT + j]) * int_to_float(hidden_layer[j]));
        }

        input_gate[i] += bias[0 * HUNIT + i];
        forget_gate[i] += bias[1 * HUNIT + i];
        cell_candidate[i] += bias[2 * HUNIT + i];
        output_gate[i] += bias[3 * HUNIT + i];

        input_gate[i] = sigmoid_function(input_gate[i]);
        forget_gate[i] = sigmoid_function(forget_gate[i]);
        cell_candidate[i] = sigmoid_function(cell_candidate[i]);
        output_gate[i] = sigmoid_function(output_gate[i]);
    }

    for (int i = 0; i < HUNIT; ++i) {

        new_cell_states[i] = float_to_int(int_to_float(forget_gate[i]) * int_to_float(cell_states [i])) + float_to_int(int_to_float(input_gate[i]) * int_to_float(cell_candidate[i]));
        new_hidden_layer[i] = float_to_int(int_to_float(output_gate[i]) * (float) (tanh((double) int_to_float(new_cell_states[i]))));

    }

    for (int i = 0; i < HUNIT; ++i) {

        hidden_layer[i] = new_hidden_layer[i];
        cell_states[i] = new_cell_states[i];
    }

    return;
}

int dense_nn(const int * input, const int * Weight, int bias) {
    int output = 0;
    for (int i = 0; i < HUNIT; ++i) {
        output += float_to_int(int_to_float(input[i]) * int_to_float(Weight[i]));
    }
    output += bias;
    return output;
}


int sigmoid_function (int input) {
    return float_to_int(1/(1+((float) exp(- (double) int_to_float(input))))); // 1/(1+exp(-(input)));
}

int float_to_int(float weight32){
    return round((weight32 - WMIN)*255/(WMAX - WMIN));
}

float int_to_float(int weight8){
	return (WMAX - WMIN)*weight8/255 + WMIN;
}




