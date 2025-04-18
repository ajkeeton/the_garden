#include "common.h"
#include "wadsworth.h"

uint32_t min_max_range_t::get_max() const {
  return avg_max;
}

uint32_t min_max_range_t::get_min() const{
  return avg_min;
}

uint32_t min_max_range_t::get_thold() const {
  // XXX Switch to stddev
  #warning revisit
  return avg_min + (avg_max - avg_min) * .25;
}

void min_max_range_t::decay() {
}

void min_max_range_t::debug() {
  Serial.printf("%u: min/max/thold: %u %u %u\n", pin, avg_min, avg_max, get_thold());
}

void min_max_range_t::update(uint16_t val) {
  uint32_t new_min_avg = avg_min, 
           new_max_avg = avg_max;

  uint32_t now = millis();
  uint32_t spread = avg_max - avg_min;

  if(val < avg_min) {
    new_min_avg = avg_min*109 + val;
    new_min_avg /= 200;
  }
  else {
    if(now - last_decay_min > TRIG_DECAY_DELTA_MIN) {
      last_decay_min = now;
      if(spread > TRIG_SPREAD_MIN)
        new_min_avg++;
    }
  }

  if(val > avg_max) {
    new_max_avg = avg_max*99 + val;
    new_max_avg /= 100;
  }
  else {
    if(now - last_decay_max > TRIG_DECAY_DELTA_MAX) {
      last_decay_max = now;
      if(spread > TRIG_SPREAD_MIN)
        new_max_avg--;
    }
  }

  // Wrapped around? Should never happen
  if(new_min_avg > 1024) {
    if(log.should_log()) {
      Serial.printf("%u: min/max/thold: %u %u %u\n", pin, new_min_avg, new_max_avg, get_thold());
      Serial.printf("%d: This should not have happened: %u\n", __LINE__, new_min_avg);
    }
    new_min_avg = avg_min;
  }

  if(new_max_avg > 1024) {
    new_max_avg = 1024;
  }

  if(new_max_avg - new_min_avg < TRIG_SPREAD_MIN)
    return;

  avg_min = new_min_avg;
  avg_max = new_max_avg;
}
