# Wav-Player

  The Project that we implemented refers to an audio streaming client–server application 
which allows multiple clients to stream audio from the server simultaneously. 


<b>How to run the Application</b>

  The application, to work correctly, we must use only Wav files which must be placed in Release>music folder. 
We have our samples in the music folder, but the user can try his own samples too.
To run the application, we have to run the two executables (AudioServer.exe, AudioClient.exe) from Release folder. 
We run the server first and after the client. After this, we have to put our server’s IP address to the client. 
If we are locally, we use the localhost IP (127.0.0.1) but If we want to use it from another pc, we use the IP from PC where the servers run.
![alt text](https://github.com/angeloskaps/Wav-Player/blob/26724c9cf9481d3255e9eb826605fcba7f76b0e3/client1.png)
  After the input of the IP, we get the music list from server by pressing L,
We can view the music list by pressing V, we can choose with sample we want to play by pressing the number of the sample (1,2 or 3) 
and we can quit the application by pressing Q.

After the chose of the sample, we can play it, stop it, pause it and go back to me main menu by pressing P, S, A, X respectively.
