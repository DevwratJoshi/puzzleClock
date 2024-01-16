/*
   Program notes:
   Global variables with a preceding underscore are flags used across functions (If this was a multi-threaded application, these would need to be protected by mutexes or atomic operators)

   Program state change rules:
   1. Initially check every second if the alarm clock has triggered the alarm by checking if ALARM_READ has been set to ground. Do not perform any other operations in this phase
   2. When the alarm clock base hardware tries to turn on the alarm, actually turn on the alarm sound
 */

#define ALARM_READ 11         // T1 collector. If this pin is HIGH, the alarm is active on the clock side
#define ALARM_READ_POWER A0   // T1 emitter. Set this pin to always HIGH.
#define ALARM_RINGER_SET A1   // Set this pin to turn the alarm ringer on/off
#define ALARM_FORCE_ON_SET 12 // Set this to high when the alarm is determined to be on from the clock side. This will make the alarm stay on till the puzzle is solved


#define RED_LED 7
#define YELLOW_LED 8
#define BLUE_LED 9
#define GREEN_LED 10

#define RED_BUTTON 3
#define YELLOW_BUTTON 4
#define BLUE_BUTTON 5
#define GREEN_BUTTON 6

#define COMMON_BUTTON_TRIGGER 2
// Need to correctly press the button this many times before the alarm stops
#define PUZZLE_COUNT 5
#define FLASH_COUNT_ON_SUCCESS 2

#define PUZZLE_INACTIVE_LOOP_TIME_MS 5000 // 5 s
#define PUZZLE_ACTIVE_LOOP_TIME_MS 5 //5 ms

#define ALARM_INACTIVE_TIME_AFTER_SOLVE_MS 1200000 // The alarm will not ring if found active again before this much time from the last time the puzzle was solved

int sleepTime = 1000; // Sleep time (s) between checking if the pinmode has changed

volatile bool     _setCheckPin       = false;
bool              _puzzleActive      = false;  // If active, then detected alarm on on clock side. Active the alarm and do not turn off until the puzzle is solved
long unsigned int _puzzleSolvedTime  = 0;
int               _remainingPuzzles  = -1;
int               _currentLightIndex = -1;

// Assume that order of pins in inputPins matches the order of pins in outputPins
const int inputPins[4]   = {RED_BUTTON, YELLOW_BUTTON, BLUE_BUTTON, GREEN_BUTTON};
const int outputPins[4]  = {RED_LED, YELLOW_LED, BLUE_LED, GREEN_LED};
const int inputPinCount  = sizeof(inputPins) / sizeof(inputPins[0]);
const int outputPinCount = sizeof(outputPins) / sizeof(outputPins[0]);
int       loopCheckTime  = PUZZLE_INACTIVE_LOOP_TIME_MS; // milliseconds. The main loop will sleep for this much time between queries. This time will be shortened if the puzzle is active 

void setup()
{
    // put your setup code here, to run once:
    pinMode(COMMON_BUTTON_TRIGGER, INPUT);
    // Assumes at least one input pin specified
    for (int i = 0; i < inputPinCount; i++) {
        pinMode(inputPins[i], INPUT);
    }

    for (int i = 0; i < outputPinCount; i++) {
        pinMode(outputPins[i], OUTPUT);
    }

    pinMode(ALARM_FORCE_ON_SET, OUTPUT);
    pinMode(ALARM_RINGER_SET, OUTPUT);
    pinMode(ALARM_READ_POWER, OUTPUT);
    digitalWrite(ALARM_RINGER_SET, LOW);
    digitalWrite(ALARM_READ_POWER, HIGH);
    randomSeed(analogRead(0));
    /*Serial.begin(115200);*/
}

// Get index from intputPins or outputPins from pin number
// For example, passing RED_LED or RED_BUTTON will return 0, which is the index associated with the red led or button in the input array and the output array
int GetIndexFromPinNumber(const int pinNumber)
{
    if (pinNumber == RED_BUTTON || pinNumber == RED_LED)
    {
        return 0;
    }
    if (pinNumber == YELLOW_LED || pinNumber == YELLOW_BUTTON)
    {
        return 1;
    }
    if (pinNumber == BLUE_BUTTON || pinNumber == BLUE_LED)
    {
        return 2;
    }
    if (pinNumber == GREEN_LED || pinNumber == GREEN_BUTTON)
    {
        return 3;
    }
}

void loop()
{
    // Check ALARM_READ. If HIGH, set BLUE to high. If LOW, set RED to high
    if (!_puzzleActive)
    {
        if (!digitalRead(ALARM_READ))
        {
            // activate alarm
            // Only turn on the alarm if we come here and we solved the alarm more than 20 mins ago. Cannot set 2 alarms too close togethe
            if (_puzzleSolvedTime == 0 || millis() - _puzzleSolvedTime > ALARM_INACTIVE_TIME_AFTER_SOLVE_MS)
            {
                _puzzleSolvedTime = 0; // reset puzzle solved time
                FlashAllLEDs();
                _puzzleActive = true;
                loopCheckTime = PUZZLE_ACTIVE_LOOP_TIME_MS;
                SetAlarm(true); // turn alarm on

                _remainingPuzzles  = PUZZLE_COUNT;
                _currentLightIndex = random(4); // Get the first light index randomly
                SetOneLight(_currentLightIndex);
            }
        }
    }
    else
    {
        int pressedButton = CheckPressedButtonAndDebounce(); // this is a pin number. Not a pin number index. Will return -1 if no button press is detected or the pressed button is not yet accepted
        if (pressedButton >= 0)
        {
            const bool res = CheckPuzzleAnswerAndContinue(GetIndexFromPinNumber(pressedButton));
            if (res)
            {
                // Finished the puzzle
                _puzzleActive = false;
                SetAlarm(false); // turn off the alarm
                FlashAllLEDs();
                _puzzleSolvedTime = millis();
                loopCheckTime = PUZZLE_INACTIVE_LOOP_TIME_MS;
            }
        }
    }
    delay(loopCheckTime);
}

// If turnOn is true, turn alarm on. If false, turn off
void SetAlarm(bool turnOn)
{
    digitalWrite(ALARM_RINGER_SET, turnOn);
    digitalWrite(ALARM_FORCE_ON_SET, turnOn);
}

void SetOneLight(const int index)
{
    TurnOffAllLEDs();
    digitalWrite(outputPins[index], HIGH);
}

// function to check the puzzle condition
// If answered correctly, reduce reduce remaining puzzle count and provide new puzzle
// If answred wrongly, reset puzzle count to max and restart
// returns false if puzzle not yet completed
// returns true if puzzle completed
bool CheckPuzzleAnswerAndContinue(const int pressedButtonIndex)
{
    if (pressedButtonIndex == _currentLightIndex)
    {
        // Answered correctly
        _remainingPuzzles -= 1;
        if (_remainingPuzzles <= 0)
        {
            return true;
        }
    }
    else
    {
        // Answered wrongly
        FlashAllLEDs();
        // reset
        _remainingPuzzles = PUZZLE_COUNT;
    }
    _currentLightIndex = ComputeNextPuzzleLightIndex(_currentLightIndex);
    SetOneLight(_currentLightIndex);
    // HAACK: wait for all the buttons to be released before returning from this function
    // This is to ensure that if we continue to press the button it is not detected as the next press
    while(digitalRead(COMMON_BUTTON_TRIGGER)) {
        delay(1);
    }
    return false;
}

// Compute the next light index for the puzzle light
int ComputeNextPuzzleLightIndex(const int currentLightIndex)
{
    static int possibleIndices[3];
    int count = 0;
    for (int i = 0; i < 4; i++) {
        if (i != currentLightIndex)
        {
            // Fill array with points other than the current index
            possibleIndices[count] = i;
            count++;
        }
    }
    return possibleIndices[random(3)];
}

void TurnOffAllLEDs()
{
    static const int pinArr[4] = {GREEN_LED, BLUE_LED, YELLOW_LED, RED_LED};
    for (int i = 0; i < 4; i++) {
        digitalWrite(pinArr[i], LOW);
    }
}

void FlashAllLEDs()
{
    static const int pinArr[4] = {GREEN_LED, BLUE_LED, YELLOW_LED, RED_LED};
    for (int i = 0; i < 4; i++) {
        digitalWrite(pinArr[i], HIGH);
    }
    delay(500);
    TurnOffAllLEDs();
}

// Check if one of the input pins has been pressed for longer than the debounce time
// If a button is detected as pressed for longer than the debounce time, return the pin number.
// Return -1 in the following cases
// 1. COMMON_BUTTON_TRIGGER is not HIGH (trivial)
// 2. A button is pressed, but has not been pressed for over the required amount of time
// 3. No button is pressed
int CheckPressedButtonAndDebounce()
{
    static int          lastPressedButton             = -1; // Button pin number, not index
    static long         lastDetectedButtonPressTimeMs = -1; // The previous time that a button was first detected as pressed
    const unsigned long debounceTimeMs                = 10; // ms. The amount of time a button has to be pressed for to be accepted as human input
    if (!digitalRead(COMMON_BUTTON_TRIGGER))
    {
        lastPressedButton             = -1;
        lastDetectedButtonPressTimeMs = -1;
        return -1;
    }
    bool buttonPressed = false;
    for (int i = 0; i < inputPinCount; i++) {
        if (digitalRead(inputPins[i]))
        {
            // If there was no button pressed the last time this function was called, or the button pressed this time is not the same as the button pressed last time, reset the debounce timer
            if (inputPins[i] != lastPressedButton)
            {
                lastDetectedButtonPressTimeMs = millis();
            }
            lastPressedButton = inputPins[i];
            buttonPressed = true;
            break;
        }
    }

    // Return -1 if no button is pressed
    if (!buttonPressed)
    {
        lastPressedButton             = -1;
        lastDetectedButtonPressTimeMs = -1;
        return -1;
    }

    // Return the current detected button if conditions are satisfied
    if (lastPressedButton >= 0 && millis() - lastDetectedButtonPressTimeMs > debounceTimeMs)
    {
        return lastPressedButton;
    }

    // If comes here, there is a button pressed but sufficient time has not elapsed. Not yet sure if this is noise or an actual button press
    return -1;
}
