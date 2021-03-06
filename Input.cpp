#include "Input.h"

#include <MY17_Can_Library.h>

#include "Pins.h"

const uint8_t BRAKE_ENGAGED_CONSTANT = 50;
const uint32_t SHUTDOWN_PERIOD_MS = 10;

// Things on motor path have shorter timeouts for safety reasons
// They transmit > 1Hz so this is safe
const uint32_t FRONT_CAN_NODE_ALIVE_TIMEOUT_MS = 1000;
const uint32_t MC_ALIVE_TIMEOUT_MS = 1000;

// Things not on motor path might have as low as 1Hz so set a 5x timeout
const uint32_t REAR_CAN_NODE_ALIVE_TIMEOUT_MS = 5000;
const uint32_t DASH_ALIVE_TIMEOUT_MS = 5000;
const uint32_t BMS_ALIVE_TIMEOUT_MS = 5000;
const uint32_t CURRENT_SENSOR_ALIVE_TIMEOUT_MS = 5000;

void update_can(Input_T *input);
void update_pins(Input_T *input);
void process_front_can_node_driver_output(Input_T *input);
void process_rear_can_node_heartbeat(Input_T *input);
void process_front_can_node_wheel_speed(Input_T *input);
void process_rear_can_node_wheel_speed(Input_T *input);
void process_dash_heartbeat(Input_T *input);
void process_dash_request(Input_T *input);
void process_bms_heartbeat(Input_T *input);
void process_bms_packstatus(Input_T *input);
void process_bms_celltemps(Input_T *input);
void process_mc_data_reading(Input_T *input);
void process_mc_state(Input_T *input);
void process_current_sensor_voltage(Input_T *input);
void process_current_sensor_current(Input_T *input);
void process_current_sensor_power(Input_T *input);
void process_current_sensor_energy(Input_T *input);
void process_unknown(Input_T *input);
bool is_alive(uint32_t last_time, uint32_t curr_time, uint32_t timeout);

void Input_fill_input(Input_T *input) {
  update_can(input);
  update_pins(input);
}

void update_pins(Input_T *input) {
  const uint32_t last_updated = input->shutdown->last_updated;
  const uint32_t next_update = last_updated + SHUTDOWN_PERIOD_MS;
  const uint32_t curr_time = input->msTicks;
  if (curr_time > next_update) {
    Shutdown_Input_T *shutdown = input->shutdown;
    shutdown->buttons_fault = !digitalRead(ESD_DRAIN_FAULT_PIN_IN);
    shutdown->bms_fault = !digitalRead(BMS_FAULT_PIN_IN);
    shutdown->imd_fault = !digitalRead(IMD_FAULT_PIN_IN);
    shutdown->bpd_fault = !digitalRead(BPD_FAULT_PIN_IN);
    shutdown->lsc_off = !digitalRead(LOW_SIDE_MEASURE_PIN_IN);
    shutdown->driver_reset = !digitalRead(DRIVER_RESET_FAULT_PIN_IN);
    shutdown->master_reset = !digitalRead(MASTER_RESET_FAULT_PIN_IN);
    shutdown->lv_voltage = analogRead(LV_MEASURE_PIN_IN);

    shutdown->last_updated = curr_time;
  }
}

void update_can(Input_T *input) {
  Can_MsgID_T msgID = Can_MsgType();
  switch(msgID) {

    case Can_FrontCanNode_DriverOutput_Msg:
      process_front_can_node_driver_output(input);
      break;

    case Can_FrontCanNode_WheelSpeed_Msg:
      process_front_can_node_wheel_speed(input);
      break;

    case Can_RearCanNode_Heartbeat_Msg:
      process_rear_can_node_heartbeat(input);
      break;

    case Can_RearCanNode_WheelSpeed_Msg:
      process_rear_can_node_wheel_speed(input);
      break;

    case Can_Dash_Heartbeat_Msg:
      process_dash_heartbeat(input);
      break;

    case Can_Dash_Request_Msg:
      process_dash_request(input);
      break;

    case Can_Bms_Heartbeat_Msg:
      process_bms_heartbeat(input);
      break;

    case Can_Bms_PackStatus_Msg:
      process_bms_packstatus(input);
      break;

    case Can_Bms_CellTemps_Msg:
      process_bms_celltemps(input);
      break;

    case Can_MC_DataReading_Msg:
      process_mc_data_reading(input);
      break;

    case Can_MC_State_Msg:
      process_mc_state(input);
      break;

    case Can_CurrentSensor_Voltage_Msg:
      process_current_sensor_voltage(input);
      break;

    case Can_CurrentSensor_Current_Msg:
      process_current_sensor_current(input);
      break;

    case Can_CurrentSensor_Power_Msg:
      process_current_sensor_power(input);
      break;

    case Can_CurrentSensor_Energy_Msg:
      process_current_sensor_energy(input);
      break;

    case Can_Unknown_Msg:
      process_unknown(input);
      break;

    case Can_Error_Msg:
      // TODO error handling
      break;

    case Can_No_Msg:
    default:
      break;
  }
}

bool Input_all_devices_alive(Input_T *input) {
  // TODO HACK actually make this work
  return true;

  bool front_can_node_alive = Input_device_alive(input, FRONT_CAN_NODE_LIVENESS);
  bool rear_can_node_alive = Input_device_alive(input, REAR_CAN_NODE_LIVENESS);
  bool bms_alive = Input_device_alive(input, BMS_LIVENESS);
  bool dash_alive = Input_device_alive(input, DASH_LIVENESS);
  bool mc_alive = Input_device_alive(input, MC_LIVENESS);
  bool current_sensor_alive = Input_device_alive(input, CURRENT_SENSOR_LIVENESS);

  return front_can_node_alive && rear_can_node_alive && dash_alive && bms_alive && mc_alive && current_sensor_alive;
}

bool Input_device_alive(Input_T *input, Liveness l) {
  switch(l) {
    case FRONT_CAN_NODE_LIVENESS:
      return is_alive(
          input->front_can_node->last_updated,
          input->msTicks,
          FRONT_CAN_NODE_ALIVE_TIMEOUT_MS
          );
    case REAR_CAN_NODE_LIVENESS:
      return is_alive(
          input->rear_can_node->last_updated,
          input->msTicks,
          REAR_CAN_NODE_ALIVE_TIMEOUT_MS
          );
    case BMS_LIVENESS:
      return is_alive(
          input->bms->last_updated,
          input->msTicks,
          BMS_ALIVE_TIMEOUT_MS
          );
    case DASH_LIVENESS:
      return is_alive(
          input->dash->last_updated,
          input->msTicks,
          DASH_ALIVE_TIMEOUT_MS
          );
    case MC_LIVENESS:
      return is_alive(
          input->mc->last_updated,
          input->msTicks,
          MC_ALIVE_TIMEOUT_MS
          );
    case CURRENT_SENSOR_LIVENESS:
      return is_alive(
          input->current_sensor->last_updated,
          input->msTicks,
          CURRENT_SENSOR_ALIVE_TIMEOUT_MS
          );
  }
  // Should never get here
  return false;
}

bool Input_shutdown_loop_closed(Input_T *input) {
  Shutdown_Input_T *shutdown = input->shutdown;
  bool has_shutdown_fault = shutdown->buttons_fault || shutdown->bms_fault || shutdown->imd_fault || shutdown->bpd_fault || shutdown->lsc_off;
  return !has_shutdown_fault;
}

void Input_initialize(Input_T *input) {
  input->front_can_node->requested_torque = 0;
  input->front_can_node->brake_pressure = 0;
  input->front_can_node->brakes_engaged = false;
  input->front_can_node->last_updated = 0;

  input->rear_can_node->last_updated = 0;

  for(int wheel = 0; wheel < NUM_WHEELS; wheel++) {
    input->speed->rpm[wheel] = 0;
    input->speed->last_updated[wheel] = 0;
  }

  input->dash->request_type = CAN_DASH_REQUEST_NO_REQUEST;
  input->dash->request_timestamp = 0;
  input->dash->last_updated = 0;

  input->bms->state = CAN_BMS_STATE_INIT;
  input->bms->last_updated = 0;
  input->bms->fan_enable = false;
  input->bms->dcdc_enable = false;
  input->bms->dcdc_fault = false;
  input->bms->highest_cell_temp_dC = 0;
  input->bms->lowest_cell_voltage_cV = 0;

  for (int i = 0; i < MC_REQUEST_LENGTH; i++) {
    input->mc->last_mc_response_times[i] = 0;
    input->mc->data[i] = 0;
  }
  input->mc->last_updated = 0;

  input->current_sensor->voltage_mV = 0;
  input->current_sensor->last_voltage_ms = 0;
  input->current_sensor->current_mA = 0;
  input->current_sensor->last_current_ms = 0;
  input->current_sensor->power_W = 0;
  input->current_sensor->last_power_ms = 0;
  input->current_sensor->energy_Wh = 0;
  input->current_sensor->last_energy_ms = 0;
  input->current_sensor->last_updated = 0;

  input->shutdown->buttons_fault = false;
  input->shutdown->bms_fault = false;
  input->shutdown->imd_fault = false;
  input->shutdown->bpd_fault = false;
  input->shutdown->lsc_off = true;
  input->shutdown->master_reset = false;
  input->shutdown->driver_reset = false;
  input->shutdown->lv_voltage = 0;
  input->shutdown->last_updated = 0;
}

void process_front_can_node_driver_output(Input_T *input) {
  Can_FrontCanNode_DriverOutput_T msg;
  Can_FrontCanNode_DriverOutput_Read(&msg);

  input->front_can_node->requested_torque = msg.torque;
  input->front_can_node->brake_pressure = msg.brake_pressure;
  input->front_can_node->brakes_engaged = msg.brake_engaged;
  input->front_can_node->last_updated = input->msTicks;
}

void process_front_can_node_wheel_speed(Input_T *input) {
  Can_FrontCanNode_WheelSpeed_T msg;
  Can_FrontCanNode_WheelSpeed_Read(&msg);

  input->speed->rpm[FL_WHEEL] = msg.front_left_wheel_speed_mRPM / 1000UL;
  input->speed->rpm[FR_WHEEL] = msg.front_right_wheel_speed_mRPM / 1000UL;
  input->speed->last_updated[FL_WHEEL] = input->msTicks;
  input->speed->last_updated[FR_WHEEL] = input->msTicks;
}

void process_rear_can_node_wheel_speed(Input_T *input) {
  Can_RearCanNode_WheelSpeed_T msg;
  Can_RearCanNode_WheelSpeed_Read(&msg);

  input->speed->rpm[RL_WHEEL] = msg.rear_left_wheel_speed_mRPM / 1000UL;
  input->speed->rpm[RR_WHEEL] = msg.rear_right_wheel_speed_mRPM / 1000UL;
  input->speed->last_updated[RL_WHEEL] = input->msTicks;
  input->speed->last_updated[RR_WHEEL] = input->msTicks;
}

void process_rear_can_node_heartbeat(Input_T *input) {
  Can_RearCanNode_Heartbeat_T msg;
  Can_RearCanNode_Heartbeat_Read(&msg);

  input->rear_can_node->last_updated = input->msTicks;
}

void process_dash_heartbeat(Input_T *input) {
  Can_Dash_Heartbeat_T msg;
  Can_Dash_Heartbeat_Read(&msg);

  input->dash->last_updated = input->msTicks;
}

void process_dash_request(Input_T *input) {
  Can_Dash_Request_T msg;
  Can_Dash_Request_Read(&msg);

  input->dash->request_type = msg.type;
  input->dash->request_timestamp = input->msTicks;

  // TODO shitcode
  // if (input->dash->request_type == CAN_DASH_REQUEST_RTD_ENABLE) {
  //   digitalWrite(MC_ENABLE_PIN_OUT, HIGH);
  // } else if (input->dash->request_type == CAN_DASH_REQUEST_RTD_DISABLE) {
  //   digitalWrite(MC_ENABLE_PIN_OUT, LOW);
  // }

  input->dash->last_updated = input->msTicks;
}

void process_bms_heartbeat(Input_T *input) {
  Can_Bms_Heartbeat_T msg;
  Can_Bms_Heartbeat_Read(&msg);

  input->bms->state = msg.state;
  input->bms->fan_enable = msg.fan_enable;
  input->bms->dcdc_enable = msg.dcdc_enable;
  input->bms->dcdc_fault = msg.dcdc_fault;

  input->bms->last_updated = input->msTicks;
}

void process_bms_packstatus(Input_T *input) {
  Can_Bms_PackStatus_T msg;
  Can_Bms_PackStatus_Read(&msg);

  input->bms->lowest_cell_voltage_cV = msg.min_cell_voltage;

  input->bms->last_updated = input->msTicks;
}

void process_bms_celltemps(Input_T *input) {
  Can_Bms_CellTemps_T msg;
  Can_Bms_CellTemps_Read(&msg);

  input->bms->highest_cell_temp_dC = msg.max_cell_temp;

  input->bms->last_updated = input->msTicks;
}

void process_mc_data_reading(Input_T *input) {
  Can_MC_DataReading_T msg;
  Can_MC_DataReading_Read(&msg);

  Can_MC_RegID_T reg = msg.type;
  MC_Request_Type type = Types_MC_Reg_to_MC_Request(reg);

  if (type != MC_REQUEST_LENGTH) {
    input->mc->last_mc_response_times[type] = input->msTicks;
    input->mc->data[type] = msg.value;
    // Serial.println("Receive Type: VCU=" + String(type) + ", MC=" + String(reg));
  }

  input->mc->last_updated = input->msTicks;
}

void process_mc_state(Input_T *input) {
  Can_MC_State_T msg;
  Can_MC_State_Read(&msg);
  input->mc->last_mc_response_times[MC_STATE] = input->msTicks;

  input->mc->state = msg;

  input->mc->last_updated = input->msTicks;
}

void process_current_sensor_voltage(Input_T *input) {
  Can_CurrentSensor_Voltage_T msg;
  Can_CurrentSensor_Voltage_Read(&msg);

  Current_Sensor_Input_T *sensor = input->current_sensor;

  sensor->voltage_mV = msg.voltage_mV;
  sensor->last_voltage_ms = input->msTicks;

  // Also update heartbeat time
  sensor->last_updated = input->msTicks;
}

void process_current_sensor_current(Input_T *input) {
  Can_CurrentSensor_Current_T msg;
  Can_CurrentSensor_Current_Read(&msg);

  Current_Sensor_Input_T *sensor = input->current_sensor;

  sensor->current_mA = msg.current_mA;
  sensor->last_current_ms = input->msTicks;

  // Also update heartbeat time
  sensor->last_updated = input->msTicks;
}

void process_current_sensor_power(Input_T *input) {
  Can_CurrentSensor_Power_T msg;
  Can_CurrentSensor_Power_Read(&msg);

  Current_Sensor_Input_T *sensor = input->current_sensor;

  sensor->power_W = msg.power_W;
  sensor->last_power_ms = input->msTicks;

  // Also update heartbeat time
  sensor->last_updated = input->msTicks;
}

void process_current_sensor_energy(Input_T *input) {
  Can_CurrentSensor_Energy_T msg;
  Can_CurrentSensor_Energy_Read(&msg);

  Current_Sensor_Input_T *sensor = input->current_sensor;

  sensor->energy_Wh = msg.energy_Wh;
  sensor->last_energy_ms = input->msTicks;

  // Also update heartbeat time
  sensor->last_updated = input->msTicks;
}

void process_unknown(Input_T *input) {
  Frame frame;
  Can_Unknown_Read(&frame);
  if (frame.id == 0x69) {
    uint8_t first = frame.data[0];
    if (first == 1) {
      // digitalWrite(MC_ENABLE_PIN_OUT, HIGH);
    } else if (first == 0) {
      // digitalWrite(MC_ENABLE_PIN_OUT, LOW);
    }
  }
}

bool is_alive(uint32_t last_time, uint32_t curr_time, uint32_t timeout) {
  if (last_time == 0) {
    return false;
  }
  uint32_t dead_time = last_time + timeout;
  return curr_time <= dead_time;
}
