All set for compile and upload DRS4Baby code

open sketch A_DRS_DEMO_I2C_LCD.ino and modifiy following line with Amazon DRS parameters relevant to your device. You are ready to upload it on your device..


//AMAZON DRS parameters
const char* device_model = "041557cb-efe4-4233-1143-93ghb6077b01"; //Product model ID of your device 
const char* device_identifier = "ABCDE12345"; // Serial No of device any thing you wish at production end
const char* client_id = "amzn1.application-oa2-client.ec9o65392ace4941a345b06e68e28e33"; // your Amazon developer account client ID
const char* client_secret = "6430ef4a824b4ee04ce69c54266dd93bb51200b08da0b03126be2f9d7857a027"; // Your Amazon developer account secret code 
const char* slot_id1_diaper = "ab754ae9-f794-4060-8aad-a8694bb87432"; //Sloat ID for Diapers
const char* slot_id2_wipes = "f22b00be-a76d-4448-99f2-304283ce1c77";  //Sloat ID for Wipes
const char* slot_id3_soap = "09403b3c-f79b-42d5-aa80-ba06f984de53";   //Sloat ID for Soap
const char* redirect_uri = "https%3A%2F%2Fdrs4baby.blogspot.in%2F2016%2F11%2Fregistration.html"; //Encoded Return URL
                           //should be encoded and same as used at time of device creation
const char* con_length = "286"; // its Packet size for post request please count your encoded redirect_uri charecters and replace this string with these number as per (286 -66 +length of your redirect URI)---yes its ugly,, but I havent updated it so far 
