#include "common.h"
#include "minmax.h"

uint32_t min_max_range_t::get_thold() const {
  //return avg_min + (avg_max - avg_min) * .25;
  //if(std_dev < MIN_STD_DEV_FOR_TRIGGER)
    //return (avg_max - avg_min) + MIN_STD_DEV_FOR_TRIGGER;
  //return (avg_max - avg_min) + std_dev;

  // XXX Review - I like the idea of using the standard div, but maybe a harcoded pct is good enough
  #if 0
  if(std_dev < MIN_STD_DEV_FOR_TRIGGER)
    return pseudo_avg + MIN_STD_DEV_FOR_TRIGGER;
  return pseudo_avg + std_dev;
  #endif

  uint32_t thold = avg_min + (avg_max - avg_min) * .075;
  return thold < MIN_THOLD ? MIN_THOLD : thold;
}

bool min_max_range_t::triggered_at(uint16_t val) {
  float sd = std_dev;
  if(sd < MIN_STD_DEV_FOR_TRIGGER)
    sd = MIN_STD_DEV_FOR_TRIGGER;
    
  // check if we're above or below the average by 1.5 std dev
  if(val > pseudo_avg + sd*2)
    // Triggered
    return true;
  
  // sd * 4 to make us less sensitive to drops in values
  if(val < pseudo_avg - sd*4)
    return true;
    
  return false;
}

void min_max_range_t::decay() {
} 

void min_max_range_t::update_window(uint16_t val) {
  window[widx++] = val;
  if(widx >= N_MOVING_AVG)
    widx = 0;
  
  pseudo_avg = 0;
  for(int i=0; i<N_MOVING_AVG; i++) {
    pseudo_avg += window[i];
  }

  pseudo_avg /= N_MOVING_AVG;
  float variance = 0;
  // Calc variance
  for(int i=0; i<N_MOVING_AVG; i++) {
    float diff = window[i] - pseudo_avg;
    variance += diff * diff;
  }
  variance /= N_MOVING_AVG;
  std_dev = sqrt(variance);
}

// Take a raw sensor value and calc:
// - new min/max range
// - standard deviation 
// - threshold
void min_max_range_t::update(uint16_t val) {
  // Aprox moving average
  // XXX Should this really use a count of the number of samples since the last window update?

  uint32_t now = millis();
  if(now - t_last_update_window > T_CALC_STD) {
    t_last_update_window = now;
    update_window(val);
  }

  float new_min_avg = avg_min, 
          new_max_avg = avg_max;

  uint32_t spread = avg_max - avg_min;

  if(val < avg_min) {
    // Aprox moving average for min
    new_min_avg = avg_min*299 + val;
    new_min_avg /= 300;
  }
  else {
    if(now - last_decay_min > TRIG_DECAY_DELTA_MIN) {
      last_decay_min = now;
      if(spread > TRIG_SPREAD_MIN)
        new_min_avg++;
    }
  }

  if(val > avg_max) {
    // Aprox moving average for max
    new_max_avg = avg_max*99 + val;
    new_max_avg /= 100;
  }
  else {
    if(now - last_decay_max > TRIG_DECAY_DELTA_MAX) {
      last_decay_max = now;
      //if(spread > TRIG_SPREAD_MIN)
      //  new_max_avg--;
      if(new_max_avg > max_max)
        new_max_avg--;
    }
  }

  // Wrapped around? Should never happen
  if(new_min_avg > 1024) {
    if(log.should_log()) {
      Serial.printf("Min/max/thold: %u %u %u\n", new_min_avg, new_max_avg, get_thold());
      Serial.printf("%d: This should not have happened: %u\n", __LINE__, new_min_avg);
    }
    new_min_avg = avg_min;
  }

  if(new_max_avg > max_max) {
    new_max_avg = max_max;
  }

  if(new_max_avg - new_min_avg < TRIG_SPREAD_MIN)
    return;

  avg_min = new_min_avg;
  avg_max = new_max_avg;
}

void min_max_range_t::log_info() {
  if(!log.should_log())
    return;

  Serial.printf("Avg: %.3f | Std dev: %.3f "
      "| Min Avg: %.3f | Max Avg: %.3f | Thold: %lu\n", 
      pseudo_avg, std_dev, avg_min, avg_max, get_thold());
}
