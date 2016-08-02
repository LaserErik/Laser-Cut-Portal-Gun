/*
Erik's LASERCUT PORTAL GUN program
This program runs the sound and lights for the ARGUS portal gun replica
It has:
-an elaborate boot sequence
-separate red and blue firing animations
Known issues:
>At higher volumes, the boot sequence stutters and fails 35-40 seconds in.
This is because the end of the song is loud, so the audio amp draws a lot of current here.
The current draw makes the battery voltage dip below the required 5.5V for the Arduino voltage regulator, inducing problems.
Adding more batteries should fix the problem (and will also increase the maximum volume!).
-Added another D-cell, problem is mostly fixed.  Would be better to use Li-Ion recharagables in the future.
 */

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
////////WAV Shield setup
#include <WaveHC.h>
#include <WaveUtil.h>
SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the volumes root directory
FatReader f;      // This holds the information for the file we're play
WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time

uint8_t dirLevel; // indent level for file/dir names    (for prettyprinting)
dir_t dirBuf;     // buffer for directory reads

#define error(msg) error_P(PSTR(msg))
void play(FatReader &dir);
///////end of WAV shield setup

long brightness = 255;
boolean red;
boolean fire;
boolean arm;
boolean armFirstCycle;
boolean systemInitialized;

//////Neopixel setup
// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(80, 7, NEO_GRB + NEO_KHZ800);
//Neopixel sets:
//0: Top indicator
//1-16: Right side strip
//17-32: Left side strip
//33-56: Front, outer ring (24pix)
//57-72: Front, middle ring (16pix)
//73-79: Front, inner ring.  (7pix).   73 = center

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

void setup() {
  //Serial.begin(9600);
  //////WAV SHIELD INITIALIZATION
  //putstring_nl("Testing syntax for simultaneous sound and light");
  //putstring("Free RAM: ");       // This can help with debugging, running out of RAM is bad
  //Serial.println(freeRam());
  
  if (!card.init()) {  //Note that this initializes the card!    
    //putstring_nl("Card init. failed!");  // Something went wrong, lets print out why
    sdErrorCheck();
    while(1);                            // then 'halt' - do nothing!
  }
  // enable optimize read - some cards may timeout. Disable if you're having problems
  card.partialBlockRead(true);
  
  // Now we will look for a FAT partition!
  uint8_t part; //Define memory address
  for (part = 0; part < 5; part++) {     // we have up to 5 slots to look in
    if (vol.init(card, part)) 
      break;                             // we found one, lets bail
  }
  if (part == 5) {                       // if we ended up not finding one  :(
    //putstring_nl("No valid FAT partition!");
    sdErrorCheck();      // Something went wrong, lets print out why
    while(1);                            // then 'halt' - do nothing!
  }
  // Lets tell the user about what we found
  //putstring("Using partition ");
 // Serial.print(part, DEC);
 // putstring(", type is FAT");
  //Serial.println(vol.fatType(),DEC);     // FAT16 or FAT32?
  
  // Try to open the root directory
  if (!root.openRoot(vol)) {
    //putstring_nl("Can't open root dir!"); // Something went wrong,
    while(1);                             // then 'halt' - do nothing!
  }
  
  // Whew! We got past the tough parts.
 // putstring_nl("Ready!"); //We've created the FatReader object 'root' at volume 'vol' in the card.
  ///////// END WAV SHIELD INITIALIZATION

  pinMode(7, OUTPUT); //Neopixel LED output
  strip.begin(); //Initialze the strip object
  strip.setBrightness(analogRead(A2)/4);
  strip.show(); // Initialize all pixels to 'off'
  systemInitialized = false;
  digitalWrite(6, HIGH); //turn on the power LED.
  red = true;
  arm = false;
  fire = false;
}

void loop() {
  if(!systemInitialized){
     if(analogRead(A3)<3){ //This is true if no light reaches the photorsistors. So, the power cell has been installed.
      bootSequence();
      systemInitialized = true;
    }
    delay(200);
  }else{
    //Read fire and arm state
    fire = analogRead(A0)>500;
    arm = analogRead(A1)>500;
    if(fire && red){
      //Fire a red portal
      fireRed();
    }else if(fire && !red){
      //Fire a blue portal
      fireBlue();
    }else if(arm && armFirstCycle){
      //Change the portal color, but only once per button press
      red = !red;
      armFirstCycle = false;
      if(red){
        strip.setPixelColor(0,0x00FF00);
      } else { //blue is only other option
        strip.setPixelColor(0,0x0000FF);
      }
      strip.show();
    }else if(!arm){
      armFirstCycle = true; //reset this boolean only if the arm button is not pressed
    }
    delay(20);
    //Right now, the code triggers on the leading edge of the button press.
    //Holding down the fire button works fine - the gun will just fire again as soon as the previous 
    //animation is complete.
    //Holding down the arm button should do nothing - the color will change once only.
    //Only after you release the button can you swap the color again.
  }
  
  //Serial.println(analogRead(A1));
  //delay(100);
  //A0 (FIRE) is 1023 if pressed. 0 if not pressed. Same for A1 (ARM)
}

void fireRed(){
  //Total track length is 0.7s
  //Most of the sound happens during the first 0.3s
  //From testing: a forced delay of 100ms was about the threshold required for it to feel responsive.
  //
  playfile("FIRERED.WAV");
  for(int i = 0; i<19; i++){
    strip.setPixelColor(max(16-i,1),0xFF0000);
    strip.setPixelColor(min(17+i,32),0XFF0000);
    if(i>3){
      strip.setPixelColor(max(16+4-i,1),0x000000);
      strip.setPixelColor(min(17-4+i,32),0x000000);
    }
    if(i==0){
      setPixels(33,56,0xFF0000); //outer ring on
    }
    if(i==5){
      setPixels(33,56,0x000000); //outer ring off
    }
    if(i==4){
      setPixels(57,72,0xFF0000); //middle ring on
    }
    if(i==10){
      setPixels(57,72,0x000000); //middle ring off
    }
    if(i==9){
      setPixels(74,79,0xFF0000); //inner ring on
       
    }
    if(i==15){
      setPixels(74,79,0x000000); //inner ring off
    }
    if(i==14){
      strip.setPixelColor(73,0xFF0000); //Center Pixel On 
    }
    strip.show();
    delay(5); //18 points at 5ms each = 90ms total length.  Nice and snappy.
  }
  for(int i = 0; i<16; i++){
    strip.setPixelColor(16-i,0x000000);
    strip.setPixelColor(17+i,0X000000);
    strip.setPixelColor(73,0x000000);
  }
  strip.show();
  //delay(100);
}

void setPixels(int startingPixel, int endingPixel, uint32_t color){ //inclusive start, end.  no show() statement.
  for(int i = startingPixel; i<=endingPixel;i++){
    strip.setPixelColor(i,color);
  }
}
/*How to allocate the front ring colors among times?  4 rings, 18 points => about 5 points each
0:Out on
1:
2:
3:
4:Mid On
5:Out off
6:
7:
8:
9:Inner On
10:Mid Off
11:
12:
13:
14:Center on
15:Inner Off
16:
17:
(18): Center off   (no actual 16th point, but included in the pixel clear at the end.
*/

void fireBlue(){
  playfile("FIREBLUE.WAV");
  for(int i = 0; i<19; i++){
    strip.setPixelColor(min(1+i,16),0x0000FF);
    strip.setPixelColor(max(32-i,17),0X0000FF);
    if(i>3){
      strip.setPixelColor(min(1+i-4,16),0x000000);
      strip.setPixelColor(max(32-i+4,17),0x000000);
    }
    if(i==0){
      //setPixels(33,56,0X0000FF); //outer ring on
      strip.setPixelColor(73,0X0000FF); //Center Pixel On 
    }
    if(i==5){
      //setPixels(33,56,0x000000); //outer ring off
      strip.setPixelColor(73,0X000000); //Center Pixel off
    }
    if(i==4){
      //setPixels(57,72,0X0000FF); //middle ring on
      setPixels(74,79,0X0000FF); //inner ring on
    }
    if(i==10){
      //setPixels(57,72,0x000000); //middle ring off
      setPixels(74,79,0X000000); //inner ring off
    }
    if(i==9){
      //setPixels(74,79,0X0000FF); //inner ring on
      setPixels(57,72,0X0000FF); //middle ring on
    }
    if(i==15){
      //setPixels(74,79,0x000000); //inner ring off
      setPixels(57,72,0X000000); //middle ring off
    }
    if(i==14){
      //strip.setPixelColor(73,0X0000FF); //Center Pixel On 
      setPixels(33,56,0X0000FF); //outer ring on
    }
    strip.show();
    delay(5); //18 points at 5ms each = 90ms total length.  Nice and snappy.
  }
  for(int i = 0; i<16; i++){
    strip.setPixelColor(16-i,0x000000);
    strip.setPixelColor(17+i,0X000000);
    setPixels(33,56,0X000000); //outer ring off
  }
  strip.show();
}

void bootSequence(){
  playfile("BOOT.WAV");
  //Total song length = 50 seconds.
  int beatTime = 250; //time per beat, in ms.
  //So, there should be 200 total beats in this sequence.
  //Pixel Color Sequence: R,G,B,RB,GB,RGB.
  uint32_t sideColorSequence[6] = {0xFF0000,0x00FF00,0x0000FF,0xFF00FF,0x00FFFF,0xFFFFFF};
  int frontPixelSequence[47];
  int pixelStatus[47];
  for(int i=0; i<47; i++){
    frontPixelSequence[i]=i;
    pixelStatus[i]=0;
  }
  int endOfArray = 46;
  setAllPixels(0x000000);
  for(int i=0; i<188; i++){
    if(analogRead(A0)>500 && analogRead(A1)>500){  //Holding both ARM and FIRE during the boot sequence will skip to the end.
      //The music is asynchronous, so it'll keep playing until you fire a portal.
      wave.stop();
      strip.setPixelColor(0,0x00FF00);
      strip.show();
      break;
    }
    //Only need to change the color of the relevant pixels, ignore the rest
    //Linear charge-up on the side LEDs:
    if(i%2==0){
       //right: 16,15,... 1
      strip.setPixelColor(max(16-i/12,1),sideColorSequence[i/2%6]);
      //left: 17, 18,...32
      strip.setPixelColor(min(17+i/12,32),sideColorSequence[i/2%6]);
    }
    //Random charge-up on front LEDs:
    if(endOfArray>=0){
      //pick a random pixel
      int selectedIndex = random(0,endOfArray);
      int selectedPixel = frontPixelSequence[selectedIndex];
      if(pixelStatus[selectedPixel] == 0){
        strip.setPixelColor(33+selectedPixel,0xFF0000);
        pixelStatus[selectedPixel] = 1;
      }else if(pixelStatus[selectedPixel] == 1){
        strip.setPixelColor(33+selectedPixel,0x00FF00);
        pixelStatus[selectedPixel] = 2;
      }else if(pixelStatus[selectedPixel] == 2){
        strip.setPixelColor(33+selectedPixel,0x0000FF);
        pixelStatus[selectedPixel] = 3;
      }else if(pixelStatus[selectedPixel] == 3){
        strip.setPixelColor(33+selectedPixel,0xFFFFFF);
        pixelStatus[selectedPixel] = 4;
        frontPixelSequence[selectedIndex] = frontPixelSequence[endOfArray];
        endOfArray--;
      }
    }
    strip.show();
    delay(beatTime);
  }
  //Set all pixels to white, then sweep down in brightness.
  //at the same time, sweep the top pixel from off to white.
  for(int i=0; i<255; i++){
    if(analogRead(A0)>500 && analogRead(A1)>500){  //Holding both ARM and FIRE during the boot sequence will skip to the end.
      strip.setPixelColor(0,0x00FF00);
      strip.show();
      break;
    }
    for(int j=1; j<strip.numPixels(); j++) {
      strip.setPixelColor(j,255-i,255-i,255-i);
    }
    strip.setPixelColor(0,0,i,0);
    strip.show();
    //5 seconds / 255 points = 19.6 ms/point.
    delay(20);
  }
}

void setAllPixels(uint32_t color){ //except the top one!
  //sets all pixels to the selected color
  for(int i=1; i<strip.numPixels(); i++) {
    strip.setPixelColor(i,color);
  }
  strip.show();
}
void setAllPixels(int R, int G, int B){
  //sets all pixels to the selected color
  for(int i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i,R,G,B);
  }
  strip.show();
}

/////WAV SHIELD FUNCTIONS
void playfile(char *name) {
  // see if the wave object is currently doing something
  if (wave.isplaying) {// already playing something, so stop it!
    wave.stop(); // stop it
  }
  // look in the root directory and open the file
  if (!f.open(root, name)) {
    putstring("Couldn't open file "); Serial.print(name); return;
  }
  // OK read the file and turn it into a wave object
  if (!wave.create(f)) {
    putstring_nl("Not a valid WAV"); return;
  }
  
  // ok time to play! start playback
  wave.play();
}

//play6 utilities
int freeRam(void){
  extern int  __bss_end; 
  extern int  *__brkval; 
  int free_memory; 
  if((int)__brkval == 0) {
    free_memory = ((int)&free_memory) - ((int)&__bss_end); 
  }
  else {
    free_memory = ((int)&free_memory) - ((int)__brkval); 
  }
  return free_memory; 
} 

void sdErrorCheck(void){
  if (!card.errorCode()) return;
  putstring("\n\rSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  putstring(", ");
  Serial.println(card.errorData(), HEX);
  while(1);
}


