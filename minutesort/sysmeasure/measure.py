from __future__ import print_function
from time import sleep
import os
import sys
import fcntl


def calculateCPUUtilization(CPULine, previousIdle, previousTotal):
    times = CPULine.split()
    idleTime = int(times[4])
    totalTime = 0
    
    for timeInt in times[1:]:
        totalTime += int(timeInt)

    idleChange = idleTime - previousIdle
    totalChange = totalTime - previousTotal
    previousIdle = idleTime
    previousTotal = totalTime
    return 100.0 * min((1.0 - idleChange / totalChange) ,1.0), idleTime, totalTime

def logCPUUtilization(CPUOutFile, CPUCorePreviousIdle, CPUCorePreviousTotal, firstRun):
    CPUStatFile = open("/proc/stat", "r")

    
    totalCPUTimes = CPUStatFile.readline()
    CPUCoreUtilizations = []
    i = 0

    while totalCPUTimes[0:3] == "cpu":
        
        if i >= len(CPUCoreUtilizations):
            CPUCorePreviousIdle.append(int(0))
            CPUCorePreviousTotal.append(int(0))
            CPUCoreUtilizations.append(int(0))

        CPUCoreUtilizations[i], newIdle, newTotal = calculateCPUUtilization(totalCPUTimes, CPUCorePreviousIdle[i], CPUCorePreviousTotal[i])       
        CPUCorePreviousIdle[i] = newIdle
        CPUCorePreviousTotal[i] = newTotal
        i += 1
        totalCPUTimes = CPUStatFile.readline()

    if(firstRun):
        CPUOutFile.write("TOTAL CPU %,")

        for i in range(0, len(CPUCoreUtilizations) - 1):
            CPUOutFile.write("THREAD %" + str(i) + ",")
        CPUOutFile.write("\n")
    
    for coreUtilization in CPUCoreUtilizations:
        CPUOutFile.write(str(coreUtilization) + ",")

    CPUOutFile.write("\n")
    CPUStatFile.close()
    CPUOutFile.flush()
    return CPUCorePreviousIdle, CPUCorePreviousTotal


def logStorageUtilization(StorageOutFile, previouskBRead, previousKBWritten, firstRun):
    StorageStatFile = os.popen("iostat -d -k")
    headerLine = ""
    statLine = ""

    while headerLine[0:6] != "Device":
        headerLine = StorageStatFile.readline()
    
    readStatsIndex = 0
    writeStatsIndex = 0
    
    for i in range(0, len(headerLine.split())):
        if headerLine.split()[i] == "kB_read":
            readStatsIndex = i
        
        if headerLine.split()[i] == "kB_wrtn":
            writeStatsIndex = i
        

    devices = []
    kBReadChange = []
    kBwrtnChange = []
    i = 0
    statLine = StorageStatFile.readline()

    while len(statLine) > 0:
        stats = statLine.split()

        if(len(stats) < 1):
            break
        devices.append(stats[0])

        if(i <= len(previouskBRead)):
            previouskBRead.append(float(stats[readStatsIndex].replace(',','.')))
            previousKBWritten.append(float(stats[writeStatsIndex].replace(',','.')))

        kBReadChange.append(float(stats[readStatsIndex].replace(',','.')) - previouskBRead[i])
        kBwrtnChange.append(float(stats[writeStatsIndex].replace(',','.')) - previousKBWritten[i])
        previouskBRead[i] = float(stats[readStatsIndex].replace(',','.'))
        previousKBWritten[i] = float(stats[writeStatsIndex].replace(',','.'))
        i += 1
        statLine = StorageStatFile.readline()
    
    if(firstRun):
        
        for device in devices:
            StorageOutFile.write("DEVICE:" + device + " bytes read/s" + ",")
            StorageOutFile.write("DEVICE:" + device + " bytes write/s" + ",")
        StorageOutFile.write("\n")
   
    for i in range(0, len(kBReadChange)):
        StorageOutFile.write(str(kBReadChange[i] * 1024) + ",")
        StorageOutFile.write(str(kBwrtnChange[i] * 1024) + ",")
    StorageOutFile.write("\n")
    StorageStatFile.close()
    StorageOutFile.flush()
    return previouskBRead, previousKBWritten


def logNetworkUtilization(networkOutFile, previousBytesRX, previousBytesTX, firstRun):
    statFile = os.popen("ip -s -o link")
    devices = []
    bytesRXChange = []
    bytesTXChange = []
    
    i = 0
    statLine = statFile.readline()

    while statLine != "":
        
        RXbytes = int(statLine.split()[27])
        TXbytes = int(statLine.split()[42])
        
        devices.append(statLine.split()[1])

        if(i <= len(previousBytesRX)):
            previousBytesRX.append(RXbytes)
            previousBytesTX.append(TXbytes)

        bytesRXChange.append(RXbytes - previousBytesRX[i])
        bytesTXChange.append(TXbytes - previousBytesTX[i])
        previousBytesRX[i] = RXbytes
        previousBytesTX[i] = TXbytes
        i += 1
        statLine = statFile.readline()

    if(firstRun):
        
        for device in devices:
            networkOutFile.write("DEVICE:" + device + "receive B/s" + ",")
            networkOutFile.write("DEVICE:" + device + "send B/s" + ",")
        networkOutFile.write("\n")
   
    for i in range(0, len(bytesRXChange)):
        networkOutFile.write(str(bytesRXChange[i]) + ",")
        networkOutFile.write(str(bytesTXChange[i]) + ",")
    networkOutFile.write("\n")
    statFile.close()
    networkOutFile.flush()
    return previousBytesRX, previousBytesTX

def logMemoryUtilization(memoryOutFile, firstRun):
    statFile = os.popen("cat /proc/meminfo")
    MemTotalKB = statFile.readline().split()[1]
    MemFreeKB = statFile.readline().split()[1]
    MemAvailableKB = statFile.readline().split()[1]
    statFile.close()
 
    if(firstRun):
        memoryOutFile.write("MEMORY UTILIZATION B,\n")
    
    memoryUtilization = (int(MemTotalKB) - int(MemAvailableKB)) * 1024
    memoryOutFile.write(str(memoryUtilization) + ",\n")
    memoryOutFile.flush()




timeMeanings = ["none", "user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal", "guest", "guest_nice"]

if len(sys.argv) < 2:
    print("Usage: python3 measure.py [node id]")


workerName = sys.argv[1]
os.makedirs(os.path.dirname("./" + workerName + "/empty.txt"), exist_ok=True)
lockFile = open("./" + workerName + "/measurements.LOCK", "w+")
fcntl.flock(lockFile.fileno(), fcntl.LOCK_EX)
CPUOutFile = open("./" + workerName + "/cpu.csv", "w+")
networkOutFile = open("./" + workerName + "/network.csv", "w+")
storageOutFile = open("./" + workerName + "/storage.csv", "w+")
memoryOutFile = open("./" + workerName + "/memory.csv", "w+")

previousIdle = []
previousTotal = []
CPUCorePreviousIdle = []
CPUCorePreviousTotal = []
previouskBRead = []
previouskBWritten = []
previousBytesRX = []
previousBytesTX = []

a = 0

while True:
    try:
        previouskBRead, previouskBWritten = logStorageUtilization(storageOutFile, previouskBRead, previouskBWritten, a == 0)
        previousIdle, previousTotal = logCPUUtilization(CPUOutFile, previousIdle, previousTotal, a == 0)
        previousBytesRX, previousBytesTX = logNetworkUtilization(networkOutFile, previousBytesRX, previousBytesTX, a == 0)
        logMemoryUtilization(memoryOutFile, a == 0)
        a += 1
        sleep(1)
    except KeyboardInterrupt:
        fcntl.flock(lockFile.fileno(), fcntl.LOCK_UN)
        sys.exit()


# for networking, see ip -s link
