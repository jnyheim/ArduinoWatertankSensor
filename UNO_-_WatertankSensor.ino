#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x3F, 16, 2);
//LiquidCrystal_I2C lcd(0x27, 20, 4);

#define RELAY1  8
#define BUTTON_A 13
#define BUTTON_B 12

bool debug = false;

int loop_spd = 500; // Loop speed in ms

int current_waterlevel = 0;
int printed_waterlevel = 0;
int max_waterlevel = 1000;
int min_waterlevel_warning = 130;   // 130 > 2 barer på lcd
float percent = 0;
int sensor_max_value = 1009;
int sensor_min_value = 200;
int sensor_value = 0;
int prev_sensor_value = 0;
float dead_switch = 2;
int claibrate_l_spd = 30;
int new_sensor_val = 0;

int button_A_state = 0;
int button_B_state = 0;

int promAddr = 0;
int promVal = 0;

bool first_run = false;
bool switch_ready = true;
bool save = false;

enum displayMode{
  displaySplash,
  displayMain,
  displayMin,
  displayMax,
  displayRaw
} display_state = displaySplash;


void setup(){
  pinMode(RELAY1, OUTPUT);
  digitalWrite(RELAY1, HIGH);
  pinMode(BUTTON_A, INPUT);
  pinMode(BUTTON_B, INPUT);
  
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  
  Serial.begin(9600);
  
  int number_1 = EEPROM.read(0);
  int number_2 = EEPROM.read(1);
  
  sensor_min_value = number_1 * 4;
  sensor_max_value = number_2 * 4;

};

void switchMenu(){
  switch_ready = false;
  switch(display_state){
    case displayMain:   setNextMenu(displayMin); break;
    case displayMin:    setNextMenu(displayMax); break;
    case displayMax:    setNextMenu(displayRaw); break;
    case displayRaw:    setNextMenu(displayMain);break;
  };
};

void setNextMenu(int next){
  // IF unsaved calibration => save.
  if(save){
    
    if(promAddr == 0){
      sensor_min_value = promVal;
    } else {
      sensor_max_value = promVal;
    }
    

    int EEval = promVal / 4;
    EEPROM.write(promAddr, EEval);
    save = false;
  };
  
  first_run = true;
  display_state = next;
};


/* #############
   # Main Loop #
   ############# */
void loop(){
  button_A_state = digitalRead(BUTTON_A);
  button_B_state = digitalRead(BUTTON_B);

  if(button_A_state && button_B_state){
    if(switch_ready){
      switchMenu();
    };
  };

  if(!switch_ready && !button_A_state && !button_B_state){
    switch_ready = true;
  }

  if(debug){
    Serial.print("Button State: ");
    Serial.print(button_A_state);
    Serial.print(button_B_state);
    Serial.println();
  };

  switch(display_state){
    case displaySplash:   display_splash();         break;
    case displayMain:     display_main();           break;
    case displayMin:      display_minWaterlevel();  break;
    case displayMax:      display_maxWaterlevel();  break;
    case displayRaw:      display_raw_value();      break;
  };
};

/* ######################
   # Splash screen Loop #
   ###################### */
void display_splash(){
  lcd.clear();
  lcd.setCursor (0, 0);
  lcd.print("Sensor System 2");
  lcd.setCursor(0, 1);
  lcd.print("By: Jonas Nyheim");
  delay(2000);

  first_run = true;
  display_state = displayMain;
  
  digitalWrite(RELAY1, LOW);
  delay(50);
  digitalWrite(RELAY1, HIGH);

};


/* #####################
   # Main display loop #
   ##################### */
void display_main(){
  if(first_run){
    lcd.clear();
    lcd.setCursor (4, 0); 
    lcd.print("L");
    lcd.setCursor (15, 0); 
    lcd.print("%");
    first_run = false;
  }
  fetch_sensor_value();

  /*
  if(debug){
    Serial.print("Sensor value: ");
    Serial.print(sensor_value);
    Serial.println();
  };
  */
  
  if(sensor_value >= prev_sensor_value + dead_switch || sensor_value <= prev_sensor_value - dead_switch ){    // execute if value is outside deadswitch
    current_waterlevel = map(sensor_value, sensor_min_value, sensor_max_value, 0, max_waterlevel);            // Map the sensor values to what we need
    percent = (float)current_waterlevel / max_waterlevel * 100.0;                                             // Calculate the percentage

    prev_sensor_value = sensor_value;                                                                         // Update the value
    
  };
  
  if(printed_waterlevel != current_waterlevel){                       // If displayed waterlevel != actual waterlevel
    print_waterlevel();                                               // Print the actual waterlevel values
    print_waterlevel_bar(1,current_waterlevel,0,max_waterlevel);      // Print the levelbar
               
    warn_low_waterlevel();                                            // If waterlevel is too low => enable 
    
    printed_waterlevel = current_waterlevel;                          // Update variable
  };
  delay(loop_spd);
};


/* ######################
   # Calibrate Min Loop #
   ###################### */
void display_minWaterlevel(){
  if(first_run){
    lcd.clear();
    new_sensor_val = sensor_min_value;
    first_run = false;
  };
  
  if(button_A_state){
    new_sensor_val += 1;
    clear_cal_val();
  };
  
  if(button_B_state){
    new_sensor_val -= 1;
    clear_cal_val();
  };

  if(new_sensor_val != sensor_min_value){
    save = true;
    promAddr = 0;
    promVal  = new_sensor_val;
  };
  
  lcd.setCursor (0, 0);
  lcd.print("Calibrate Min");
  lcd.setCursor (0, 1);
  lcd.print("value: ");
  lcd.print(new_sensor_val);

  delay(claibrate_l_spd);

};


/* ######################
   # Calibrate Max Loop #
   ###################### */
void display_maxWaterlevel(){
  if(first_run){
    lcd.clear();
    new_sensor_val = sensor_max_value;
    first_run = false;
  };
  
  if(button_A_state){
    new_sensor_val += 1;
    clear_cal_val();
  };
  
  if(button_B_state){
    new_sensor_val -= 1;
    clear_cal_val();
  };

  if(new_sensor_val != sensor_max_value){
    save = true;
    promAddr = 1;
    promVal  = new_sensor_val;
  };
  
  lcd.setCursor (0, 0);
  lcd.print("Calibrate Max");
  lcd.setCursor (0, 1);
  lcd.print("value: ");
  lcd.print(new_sensor_val);
  delay(claibrate_l_spd);
};

/* ########################
   # Display Raw val loop #
   ######################## */
void display_raw_value(){
  if(first_run){
    lcd.clear();
    first_run = false;
  };

  lcd.setCursor(0,0);
  lcd.print("Raw Sensor value:");
  lcd.setCursor(0,1);
  fetch_sensor_value();
  lcd.print(sensor_value);

  delay(loop_spd);
};

void fetch_sensor_value(){
  sensor_value = analogRead(A0);           // Read sensor value from analog pin 0
}


/* Enable or disable the warning light/relay */
void warn_low_waterlevel(){
  if(current_waterlevel < min_waterlevel_warning){
    digitalWrite(RELAY1,LOW);
    
  } else {
    digitalWrite(RELAY1,HIGH);
    
  };
};

/* Update the waterlevel values on LCD */
void print_waterlevel(){
  clear_values();
  
  lcd.setCursor (0, 0);
  lcd.print(current_waterlevel);

  lcd.setCursor (9, 0);
  lcd.print(percent);
};

/* Clear the waterlevel values on LCD */
void clear_values(){
  lcd.setCursor (0, 0);
  lcd.print("    ");
  lcd.setCursor (9, 0);
  lcd.print("      ");
};

// 
void clear_cal_val(){
  lcd.setCursor(7,1);
  lcd.print("    ");
}








void print_waterlevel_bar (int row, int var, int minVal, int maxVal){
  int block = map(var, minVal, maxVal, 0, 16);                  // Block represent the current LCD space (modify the map setting to fit your LCD)
  int line = map(var, minVal, maxVal, 0, 80);                   // Line represent the theoretical lines that should be printed 
  int bar = (line-(block*5));                                   // Bar represent the actual lines that will be printed
  
  /* LCD Progress Bar Characters, create your custom bars */

  byte bar1[8] = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10};
  byte bar2[8] = { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18};
  byte bar3[8] = { 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C};
  byte bar4[8] = { 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E};
  byte bar5[8] = { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};
  lcd.createChar(1, bar1);
  lcd.createChar(2, bar2);
  lcd.createChar(3, bar3);
  lcd.createChar(4, bar4);
  lcd.createChar(5, bar5);
  
  for (int x = 0; x < block; x++)                               // Print all the filled blocks
  {
    lcd.setCursor (x, row);
    lcd.write (1023);
  }
  
  lcd.setCursor (block, row);                                   // Set the cursor at the current block and print the numbers of line needed
  if (bar != 0) lcd.write (bar);
  if (block == 0 && line == 0) lcd.write (1022);                // Unless there is nothing to print, in this case show blank
  
  for (int x = 16; x > block; x--)                              // Print all the blank blocks
  {
    lcd.setCursor (x, row);
    lcd.write (1022);
  }
};
