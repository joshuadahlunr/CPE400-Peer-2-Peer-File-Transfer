# CPE400 Networking Project

Whisper Network

## Team Members

Joshua Dahl

Antonio Massa

Annette McDonough

https://github.com/joshuadahlunr/CPE400-Peer-2-Peer-File-Transfer

## Write up

We implemented a file sharing network using a system of peer-to-peer connections.

## Build

**NOTE: This project has been built using c++17, thus a compiler capable of supporting that version is required. Any modern linux compiler should be supported. The project hasn't been tested on windows but it should (theoretically) work fine.**

From a command console

    git clone https://github.com/joshuadahlunr/CPE400-Peer-2-Peer-File-Transfer
    git submodule init
    git submodule update --init --recursive

Create a build directory
Move to that build directory

    cmake ..
    make
    ./wnts

Or you can copy and paste this code block in a terminal.

    mkdir build
    cd build
    cmake ..
    make
    ./wnts

### Controls:

  **NOTE: A demo of the program running (demo.mp4) is included.**

  **Once program is built run with**
  **to establish the first peer node**
    
    ./wnts -f *file name here*

  **to establish a peer connection on a seperate device**

    ./wnts -c *gateway node IPv6 address here (will be displayed on the first peer)*
    
    
  **To end a peer conection on both devices**

   ctrl + c 

