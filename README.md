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

From a linux command console

    git clone https://github.com/joshuadahlunr/CPE400-Peer-2-Peer-File-Transfer
    git submodule init
    git submodule update --init --recursive

Create a build directory
Move to that build directory

    Run cmake ..
    Run make
    Run ./wnts

Or you can copy and paste this code block in the terminal from PA10

    mkdir build
    cd build
    cmake ..
    make
    ./wnts

### Controls:

  **Once program is built run with**
  **to establish the first peer node**
    
    ./wnts -f *file name here*

  **to establish a peer connection**

    ./wnts -c *gateway node IPv6 address here*

