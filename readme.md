# FanControl4MSI
This program can control the fans of a MSI mainboard. It was tested with the H97 Gaming 3 but should work on all other ones using the MSI Command Center (which must be installed to use this program). FanControl4MSI can be used to circumvent GUI restrictions (like the minimum 50%) or to set specific values for fan speed at logon. 

## Running
The program only takes one type of argument but multiple times. It looks like ```<FAN>;<S1>;<S2>;<S3>;<S4>;<T1>;<T2>;<T3>;<T4>;77``` where all <> are variables.
* \<FAN\> is the fan id. 1 and 2 are most likely the cpu fans do dont touch them! In most cases 8 and 9 and 3 are case fans which are the fans for me on H97. I have not really a clue how you could figure the ids easily out and I am also not sure if you can damage something when entering a wrong one.
* \<Sx\> is the desired speed of the fan and \<Tx\> is the temperature. The 'x' is the number of the 'Ball' you can move in the Command Center, numbered from the left to the right. However, the \<Sx\> and \<Tx\> values need to be entered as HEX (uppercase) and not as decimal numbers.

As a little example, this would set my one of my front fans to an acceptable speed:<br>
```8;1E;23;2D;3C;2D;37;41;50;77;``` <br>
```[30/45],[35,55],[45,65],[55,80] where [Speed/Temp]```<br>
The last 77 might be the temp where the 100 speed triggers but this is only speculation.<br>
By specifying this string multiple times you can set the speed for multiple fans at once. With the help of the windows scheduler this program now gets called at every login with <br>```3;1E;23;2D;3C;2D;37;41;50;77; 8;1E;23;2D;3C;2D;37;41;50;77; 9;1E;23;2D;3C;2D;37;41;50;77;```<br> as arguments. As you might be able to tell, I set all fans to the same settings by specifying the one argument multiple times (separated by a space) only changing the fan id.

This completly bypasses the GUI and does not set anything for it so it might show nothing or completely off values.

## Building
For building I used MinGW64.
This program uses a slightly modified version of https://github.com/taneryilmaz/libconfigini/ to be able to read and write the ini files that are used by the service. The changes are that the library no longer throws an error on empty values but simply accepts them.

