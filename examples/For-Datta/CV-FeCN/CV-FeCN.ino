#include <Wire.h>
#include <LMP91000.h>


LMP91000 pStat = LMP91000();


const uint16_t opVolt = 3330;
const uint8_t adcBits = 12;
const double v_tolerance = 0.0075;
const uint16_t dacResolution = pow(2,12)-1; //12-bit


//analog input pins to read voltages
const uint8_t LMP = A3;
const uint8_t dac = A0;
const uint8_t MENB = 5;


void setup()
{
  Wire.begin();
  SerialUSB.begin(115200);

  analogReadResolution(12);
  analogWriteResolution(12);


  pStat.setMENB(MENB);


  //enable the potentiostat
  delay(50);
  pStat.standby();
  delay(50);
  initLMP(0);
  delay(2000); //warm-up time for the gas sensor
}


void loop()
{
  while(!SerialUSB);
  SerialUSB.println(F("Ready!"));
  
  //will hold the code here until a character is sent over the SerialUSB port
  //this ensures the experiment will only run when initiated
  while(!SerialUSB.available());
  SerialUSB.read();


  //prints column headings
  SerialUSB.println(F("Voltage,Zero,LMP"));


  //lmpGain, cycles, startV(mV), endV(mV), vertex1(mV), vertex2(mV), stepV(mV), rate (mV/s)
  runCV(4, 2, 0, 0, 450, -200, 5, 100);
  //SerialUSB.println("Backward Scan");
  //runCV(4, 2, 0, 0, -500, 600, 2, 100);
}


void initLMP(uint8_t lmpGain)
{
  pStat.disableFET();
  pStat.setGain(lmpGain);
  pStat.setRLoad(0);
  pStat.setExtRefSource();
  pStat.setIntZ(1);
  pStat.setThreeLead();
  pStat.setBias(0);
  pStat.setPosBias();

  setOutputsToZero();
}


void setOutputsToZero()
{
  analogWrite(dac,0);
  pStat.setBias(0);
}


void runCV(uint8_t lmpGain, uint8_t cycles, int16_t startV,
           int16_t endV, int16_t vertex1, int16_t vertex2,
           int16_t stepV, uint16_t rate)
{
  initLMP(lmpGain);
  stepV = abs(stepV);
  rate = (1000.0*stepV)/rate;


  if(vertex1 > startV) runCVForward(cycles,startV,endV,vertex1,vertex2,stepV,rate);
  else runCVBackward(cycles,startV,endV,vertex1,vertex2,stepV,rate);
}



void runCVForward(uint8_t cycles, int16_t startV, int16_t endV,
                  int16_t vertex1, int16_t vertex2, int16_t stepV, uint16_t rate)
{
  int16_t j = startV;
  
  for(uint8_t i = 0; i < cycles; i++)
  {
    //j starts at startV
    for (j; j <= vertex1; j += stepV)
    {
      biasAndSample(j,rate);
    }
    j -= 2*stepV;
  
  
    //j starts right below the first vertex
    for (j; j >= vertex2; j -= stepV)
    {
      biasAndSample(j,rate);
    }
    j += 2*stepV;
  
  
    //j starts right above the second vertex
    for (j; j <= endV; j += stepV)
    {
      biasAndSample(j,rate);
    }
    j -= 2*stepV;
    
  }

  setOutputsToZero();
}


void runCVBackward(uint8_t cycles, int16_t startV, int16_t endV,
                   int16_t vertex1, int16_t vertex2, int16_t stepV, uint16_t rate)
{  
  int16_t j = startV;
  
  for(uint8_t i = 0; i < cycles; i++)
  {
    //j starts at startV
    for (j; j >= vertex1; j -= stepV)
    {
      biasAndSample(j,rate);
    }
    j += 2*stepV;
    

    //j starts right above vertex1
    for (j; j <= vertex2; j += stepV)
    {
      biasAndSample(j,rate);
    }
    j -= 2*stepV;
  

    //j starts right below vertex2
    for (j; j >= endV; j -= stepV)
    {
      biasAndSample(j,rate);
    }
    j += 2*stepV;
    
  }

  setOutputsToZero();
}



void biasAndSample(int16_t voltage, uint16_t rate)
{
  SerialUSB.print(voltage);
  SerialUSB.print(F(","));
  
  setLMPBias(voltage);
  setVoltage(voltage);
  
  delay(rate);
  sampleOutputs();
  SerialUSB.println();
}



void sampleOutputs()
{
  //SerialUSB.print(pStat.getCurrent(analogRead(LMP),opVolt,adcBits),8);
  SerialUSB.print(pStat.getVoltage(analogRead(LMP), opVolt, adcBits));
}


void setVoltage(int16_t voltage)
{
  uint16_t dacVout = 1500;
  uint8_t bias_setting = 0;

  if(abs(voltage) < 15) voltage = 15*(voltage/abs(voltage));    
  
  int16_t setV = dacVout*TIA_BIAS[bias_setting];
  voltage = abs(voltage);
  
  
  while(setV > voltage*(1+v_tolerance) || setV < voltage*(1-v_tolerance))
  {
    if(bias_setting == 0) bias_setting = 1;
    
    dacVout = voltage/TIA_BIAS[bias_setting];
    
    if (dacVout > opVolt)
    {
      bias_setting++;
      dacVout = 1500;

      if(bias_setting > NUM_TIA_BIAS) bias_setting = 0;
    }
    
    setV = dacVout*TIA_BIAS[bias_setting];    
  }


  pStat.setBias(bias_setting);
  analogWrite(dac,convertDACVoutToDACVal(dacVout));

  SerialUSB.print(dacVout*.5);
  SerialUSB.print(F(","));
}



//Convert the desired voltage
uint16_t convertDACVoutToDACVal(uint16_t dacVout)
{
  //return (dacVout-dacMin)*((double)dacResolution/dacSpan);
  return dacVout*((double)dacResolution/opVolt);
}



void setLMPBias(int16_t voltage)
{
  signed char sign = (double)voltage/abs(voltage);
  
  if(sign < 0) pStat.setNegBias();
  else if (sign > 0) pStat.setPosBias();
  else {} //do nothing
}