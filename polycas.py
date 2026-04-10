
from argparse import ArgumentParser
from time import sleep
from sys import exit
from pathlib import Path
from datetime import timedelta
from numpy import arange, sin, pi, int16
import wave
import struct

from serial import Serial

SYNC = b'\xe6'
SOH  = b'\x01'

TYPE_DATA = b'\x00'
TYPE_COMMENT = b'\x01'
TYPE_END = b'\x02'
TYPE_EXEC = b'\x03'

class CassetteProgram:
    def __init__(self):
        # header consists of sync characters followed by Start of header
        self.header = SYNC * 16 + SOH
        self.sections = []
        self.size = 0

    def __createChecksum(self, s):
        if s == b'':
            return b''
        sum = 0
        for b in s:
            sum += b
        sum %= 0x100
        return bytes([(0x100-sum)%0x100])

    def __createSection(self, address, data, sectiontype):
        if sectiontype == TYPE_EXEC:
            execaddr = address
            address = 0
        else:
            execaddr = address
        a = self.name + \
             bytes([address % 0x100, address // 0x100]) + \
             bytes([len(data) % 0x100]) + \
             bytes([execaddr % 0x100, execaddr // 0x100]) + \
             sectiontype
        if len(data) == 0:
            b = b''
        else:
            b = data
        self.sections.append(self.header + a + self.__createChecksum(a) + b + self.__createChecksum(b))
        self.size += len(self.sections[-1])
             
    def setName(self, name):
        name = name.encode()
        if len(name) > 8:
            print("Error: program name field greater than 8 characters")
            exit(-1)
        # pad the name field with spaces up to 8 chars
        if len(name) < 8:
            name = name + (b' ' * (8-len(name)))
        self.name = name

    def createDataMessage(self, address, data):
        self.__createSection(address, data, TYPE_DATA)

    def createCommentMessage(self, s):
        self.__createSection(0x0000, s, TYPE_COMMENT)

    def createEndMessage(self):
        self.__createSection(0x0000, b'', TYPE_END)

    def createExecMessage(self, execaddr):
        self.__createSection(execaddr, b'', TYPE_EXEC)

    def loadFromBin(self, infile, loadaddr, execaddr, name):
        print('Creating cas object...')
        self.setName(name)
        self.createCommentMessage(b'\r\nLOADING START\r\n')
        with open(infile, 'rb') as f:
            address = loadaddr
            increment = 0x100
            data = f.read(increment)
            while len(data) != 0:
                self.createDataMessage(address,data)
                address += increment
                data = f.read(increment)
            self.createCommentMessage(b'LOADING END\r\n')
            if execaddr is not None:
                self.createExecMessage(execaddr)
            else:
                self.createEndMessage()

    def load(self, infile):
        print('Loading cas file...')
        with open(infile, 'rb') as f:
            self.size = 0
            self.sections = []
            index = 0
            while True:
                section = b''
                c = f.read(1)
                if c == b'':
                    break
                while c == SYNC:
                    section += c
                    c = f.read(1)
                section += c    
                c = f.read(16)
                section += c
                s = c[10]
                if s == 0:
                    length = 256
                else:
                    length = s
                #print(index, length)
                c = f.read(length)
                section += c
                index += 1
                self.sections.append(section)
                self.size += len(self.sections[-1])

    def save(self, outfile):
        print('Saving cas file...')
        with open(outfile, 'wb') as f:
            for section in self.sections:
                f.write(section)

    def saveAsWavs(self, file_path):
        samplerate = 48000.0
        t = arange(0,160.0)
        a = 30000.0
        f0 = a*sin(t*2*pi/160*4)
        f1 = a*sin(t*2*pi/160*8)
        f0 = f0.astype(int)
        f1 = f1.astype(int)
        buffers = [ struct.pack('<160h',*(f0)),
                    struct.pack('<160h',*(f1)) ]
        print('Saving wav files...')
        with wave.open(str(file_path.with_stem(file_path.stem + '_byte').with_suffix('.wav')),'wb') as wf:
            wf.setnchannels(1)   # Mono audio
            wf.setsampwidth(2)   # 2 bytes per sample (16-bit)
            wf.setframerate(samplerate)
            for section in self.sections:
                for c in section:
                    wf.writeframes(buffers[0])
                    for bit in range(0,8):
                        b = (c >> bit) & 0x01
                        wf.writeframes(buffers[b])
                    wf.writeframes(buffers[1])
                    wf.writeframes(buffers[1])
        with open(file_path.with_stem(file_path.stem + '_poly').with_suffix('.wav'),'wb') as f:
            pass        

    def stream(self, port):
        print('Sending to serial port...')
        ser = Serial(port, 300, stopbits=2, timeout=1, write_timeout=1)
        #print(ser.get_settings())
        sent_size = 0
        for section in self.sections:
            sent_size += len(section)
            self.send(ser, section)
            time_remaining = timedelta(seconds = int((self.size - sent_size) * 11 / 300))
            print(f'{sent_size/self.size*100:3.0f}% complete - time remaining: {str(time_remaining)[2:]}')
        print("Done")
        sleep(3)

    def send(self, ser, msg):
        for i in range(0,len(msg)):
            ser.write(msg[i:i+1])
            sleep(11.0/300)

def main():
    # Parse Arguments
    def auto_int(x): # function to handle ints and int literals
        return int(x,0)
    parser = ArgumentParser(description = 'polycas - Poly-88 Cassette Utility')
    parser.add_argument('-i','--infile',help='Input File',required=True)
    parser.add_argument('-o','--outfile',help='Output File',required=False)
    parser.add_argument('-n','--name',default=b' ',help='Name',required=False)
    parser.add_argument('-p','--port',help='Serial Port Name',required=False)
    parser.add_argument('-a','--addr',type=auto_int,help='Load Address',required=False)
    parser.add_argument('-e','--exec',type=auto_int,help='Exec Address',required=False)
    parser.add_argument('-w','--wav',help='Write WAV files',required=False,action="store_true")
    args = parser.parse_args()

    # Create or load cas file
    input_file_path = Path(args.infile)
    if input_file_path.suffix != '.cas':
        if args.addr is None:
            parser.print_help()
            exit(-1)
        casobj = CassetteProgram()
        casobj.loadFromBin(args.infile, args.addr, args.exec, args.name)
        if args.outfile is not None:
            casobj.save(args.outfile)
            wavbase = Path(args.outfile)
        else:
            wavbase = Path(args.infile)
    else:
        casobj = CassetteProgram()
        casobj.load(args.infile)
        wavbase = Path(args.infile)
        
    # save to wav files
    if args.wav:
        casobj.saveAsWavs(wavbase)
    # stream file to serial if desired
    if args.port:
        casobj.stream(args.port)

'''
def oldmain():
    argv = sys.argv
    if len(argv) != 4 and len(argv) != 5:
        print('Usage: polyload <filename> <startexecaddr> <comport> [recordname]')
        exit(-1)
    filename = argv[1]
    startaddr = int(argv[2],0)
    comport = argv[3]
    if len(argv) == 5:
        rname = argv[4].encode('ascii')
    else:
        rname = b' '
    ser = serial.Serial(comport, 300, stopbits=2, timeout=1)
    pgm = CassetteProgram()
    pgm.setName(name=rname)
    msg = pgm.createCommentMessage(b'\r\nLOADING START\r\n')
    Write(ser,msg)
    print("LOADING START")
    f = open(filename,'rb')
    address = startaddr
    increment = 0x100
    data = f.read(increment)
    while len(data) != 0:
        msg = pgm.createDataMessage(address,data)
        Write(ser,msg)
        print(f"{pgm.name.decode()} {address:04X}")
        address += increment
        data = f.read(increment)
    msg = pgm.createCommentMessage(b'LOADING END\r\n')
    Write(ser,msg)
    print("LOADING END")
    msg = pgm.createExecMessage(startaddr)
    Write(ser,msg)
    print("Done")
    while True:
        sleep(1)
'''

if __name__ == '__main__':
    #import serial.tools.list_ports
    #print([comport.device for comport in serial.tools.list_ports.comports()])
    main()

"""
TBD - redo this section
SAMPLE commands:
python polyload.py tetris_poly88.bin 0x1000 COM6
python polyload.py demo.bin 0x1000 COM6
python polyload.py bezier.bin 0x1000 COM6
python polyload.py lenna.bin 0x1000 COM6
python polyload.py readwritedi.bin 0x0c6a COM6
python polyload.py readwrite.bin 0x0c6a COM6
"""
