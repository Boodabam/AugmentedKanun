#!/usr/bin/env python3

import numpy as np
import serial
import json
import os
from prompt_toolkit import prompt

Nbank = 5;
Nstate = 10;
N=27;
ser = serial.Serial()
ser.port = 'COM3'
ser.baudrate = 9600


def save(filepath):
    #reading
    matrixLights = np.zeros((Nbank,Nstate))
    ser.open()
    ser.write('save'.encode())
    matrixModes = np.zeros((Nbank,Nstate,N))
    print('bien écrit save : N=',N)
    '''
    for i in range(Nbank):
        for j in range(Nstate):
            matrixLights[i,j] = ser.read(size=4)
            for k in range(N):
                matrixModes[i,j,k] = ser.read(size=4)
    '''
    #writing
    with open(filepath, 'w') as file:
        np.savetxt(file, (matrixLights, matrixModes))
    ser.close()
    return matrixLights, matrixModes

def load(filepath, N):
    #Loading file
    with open(filepath, 'r') as file:
        matrixLights, matrixModes = file.read()
    #sending to serial
    ser.open()
    ser.write('load'.encode())
    ser.read(N)
    for i in range(Nbank):
        for j in range(Nstate):
            ser.write(matrixLights[i,j])
            for k in range(N):
                ser.read(matrixModes[i,j,k])
    ser.close()
    return matrixLights, matrixModes

def calibrate():
    ser.open()
    ser.write('calibrate'.encode())
    ser.close()


print('Welcome to Kanun+ driver 0.0.1, here are the different possible operations :',end='\n')
print('- calibrate : Place all faders to their left-most position, press pedal, than place all faders to their left-most position and press pedal again.',end='\n')
print('- save file_path : Saves modes in a given file',end='\n')
print('- load file_path : Loads modes from a given file. This will erease all unsaved modes',end='\n')
print('- exit : Exits program')
while(True):
    whatShouldIDo = prompt("Enter command :")
    whatShouldIDo = whatShouldIDo.split()
    if whatShouldIDo[0] == 'calibrate':
        print('Calibration...')
        calibrate()
        print('Calibration successful...')
    elif whatShouldIDo[0] == 'save':
        print('Saving modes...')
        save(whatShouldIDo[1])
        #TODO Error path
        print('modes saved')
    elif whatShouldIDo[0] == 'load':
        print('Loading modes...')
        save(whatShouldIDo[1])
        #TODO Error no such file
        print('modes loaded')
    elif whatShouldIDo[0] == 'exit':
        break
    else:
        print('Error: \"',whatShouldIDo[0],'\" no such command.')