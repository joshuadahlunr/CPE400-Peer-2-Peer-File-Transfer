#include <fstream>
#include <string>
#include <iostream>

using namespace std;

int main() 
{  
    ofstream fout;
    string message;

    cout << "Please enter a message:\n";
    getline(cin, message, '\n');

    fout.open("test.txt", ios::app); // append instead of overwrite
    fout << message << endl; 
    cout << message << endl;
    return 0;
}