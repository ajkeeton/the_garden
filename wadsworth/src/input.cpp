#include "common.h"

extern SemaphoreHandle_t muxmtx;

uint32_t min_max_range_t::get_max() {
  return avg_max;
}

uint32_t min_max_range_t::get_min() {
  return avg_min;
}

uint32_t min_max_range_t::get_thold() {
  // XXX Switch to stddev
  return avg_min + (avg_max - avg_min) * .25;
}

void min_max_range_t::decay() {
  uint32_t now = millis();
  if(now - last_decay_min > TRIG_DECAY_DELTA_MIN) {
    last_decay_min = now;
    if(avg_max - avg_min + 1 > TRIG_SPREAD_MIN)
      avg_min++;
  }

  if(now - last_decay_max > TRIG_DECAY_DELTA_MAX) {
    last_decay_max = now;
    if(avg_max - 1 - avg_min > TRIG_SPREAD_MIN)
      avg_max--;
  }
}

void min_max_range_t::update(uint16_t val) {
  uint32_t new_min_avg = avg_min, 
           new_max_avg = avg_max;

  if(val < avg_min) {
    new_min_avg = avg_min*19 + val;
    new_min_avg /= 20;
  }

  if(val > avg_max) {
    new_max_avg = avg_max*9 + val;
    new_max_avg /= 10;
  }

  // Wrap around. Should never happen
  if(new_min_avg > 1024) {
    if(log.should_log())
      Serial.printf("%d: This should not have happened: %u\n", __LINE__, new_min_avg);
    new_min_avg = avg_min;
  }
  // This should also never happen
  if(new_max_avg > 1024) {
    if(log.should_log())
      Serial.printf("%d: This should not have happened: %u\n", __LINE__, new_max_avg);
    new_max_avg = avg_max;
  }

  // Enforce a minimum spread
  if(new_max_avg - new_min_avg < TRIG_SPREAD_MIN) {
    //if(pin == 8) Serial.printf("%u: at min spread: %u - %u < %u. Thold: %u\n", 
    //    pin, new_max_avg, new_min_avg, TRIG_SPREAD_MIN, get_thold());   
    return;
  }

  avg_min = new_min_avg;
  avg_max = new_max_avg;

  if(pin == 8) { 
    // if(log.should_log()) {
    //Serial.printf("%u: min/max/thold: %u %u %u\n", pin, avg_min, avg_max, get_thold());
  }

  decay();
}

ir_input_t::ir_input_t() {
  for(int i=0; i<N_SAMPLES; i++)
    samples[i] = 0;
}

void ir_input_t::update(uint16_t v) {
  samples[idx] = v;
  val = 0;
  for(int i=0; i<N_SAMPLES; i++)
    val += samples[i];
  val /= N_SAMPLES;
  idx >= N_SAMPLES ? idx = 0 : idx++;
}

uint16_t decay(uint16_t score) {
  // XXX Make dynamic
  if(score > 5)
    return score - 5;
  else
    return 0;
}

void sensor_t::update(uint32_t now) {
  uint16_t raw = mux->read_raw(mux_pin);

  minmax.update(raw);

  if(raw > minmax.get_thold()) {
    if(!score) {
      start = now;
      pending_pulse = true;
    }
    score = raw - minmax.get_thold();
  }

  score = decay(score);

  if(!score)
    start = 0;
}
