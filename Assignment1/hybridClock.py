"""
Author: Lee Yu Huei

Reference: https://towardsdatascience.com/understanding-lamport-timestamps-with-pythons-multiprocessing-library-12a6427881c6
"""
from multiprocessing import Process, Pipe
from os import getpid
from datetime import datetime

def convert_time_to_ms(time: str):
    minutes, seconds_and_microseconds = time.split(":")
    seconds, microseconds = map(float, str(seconds_and_microseconds).split("."))
    return round((60 * int(minutes) + int(seconds)) * 1000 + microseconds / 1000)

def hlc_timestamp(physical_time: float, logical_time: float, c_value: int):
    return '\tHLC_TIMESTAMP=({}, {}, {})\n'.format(physical_time, logical_time, c_value)

def get_physical_time():
    return convert_time_to_ms(datetime.now().strftime("%M:%S.%f"))

def local_event(pid: int, physical_time: float, logical_time: float, c_value: int):
    new_logical_time = max(physical_time, logical_time)
    if (logical_time == new_logical_time):
        c_value += 1
    if (physical_time == new_logical_time):
        c_value = 0
    
    print('process{} has a local event!\n{}'.format(str(pid), hlc_timestamp(physical_time, new_logical_time, c_value)))

    return new_logical_time, c_value

def send_event(pipe, pid: int, physical_time: float, logical_time: float, c_value: int):
    new_logical_time = max(physical_time, logical_time)
    if (logical_time == new_logical_time):
        c_value += 1
    if (physical_time == new_logical_time):
        c_value = 0

    pipe.send((pid, new_logical_time, c_value))
    print('process{} sends a message!\n{}'.format(str(pid), hlc_timestamp(physical_time, new_logical_time, c_value)))
    
    return new_logical_time, c_value

def recv_event(pipe, pid: int, physical_time: float, logical_time: float, c_value: int):
    sender, sender_logical_time, sender_c_value = pipe.recv()
    new_logical_time = max(logical_time, sender_logical_time, physical_time)
    if (logical_time == sender_logical_time == new_logical_time):
        c_value = max(c_value, sender_c_value) + 1
    elif (logical_time == new_logical_time):
        c_value += 1
    elif (sender_logical_time == new_logical_time):
        c_value = sender_c_value + 1
    if (physical_time == new_logical_time):
        c_value = 0
    
    print('process{} receives a message from process{}!\n{}'.format(str(pid), str(sender), hlc_timestamp(physical_time, new_logical_time, c_value)))
    
    return new_logical_time, c_value

def process_one(start_time, pipe12):
    pid = 1
    logical_time, c_value = 0, 0
    logical_time, c_value = send_event(pipe12, pid, start_time + 1000, logical_time, c_value)
    logical_time, c_value  = recv_event(pipe12, pid, get_physical_time() + 1000, logical_time, c_value)

def process_two(start_time, pipe21, pipe23, pipe24):
    pid = 2
    logical_time, c_value = 0, 0
    logical_time, c_value  = local_event(pid, start_time, logical_time, c_value)
    logical_time, c_value = recv_event(pipe21, pid, get_physical_time(), logical_time, c_value)
    logical_time, c_value = send_event(pipe23, pid, get_physical_time(), logical_time, c_value)
    logical_time, c_value = recv_event(pipe24, pid, get_physical_time(), logical_time, c_value)
    logical_time, c_value = recv_event(pipe24, pid, get_physical_time(), logical_time, c_value)
    logical_time, c_value = send_event(pipe21, pid, get_physical_time(), logical_time, c_value)

def process_three(start_time, pipe32, pipe34):
    pid = 3
    logical_time, c_value = 0, 0
    logical_time, c_value = send_event(pipe34, pid, start_time + 1000, logical_time, c_value)
    logical_time, c_value = recv_event(pipe32, pid, get_physical_time() + 1000, logical_time, c_value)
    logical_time, c_value = send_event(pipe34, pid, get_physical_time() + 1000, logical_time, c_value)

def process_four(start_time, pipe43, pipe42):
    pid = 4
    logical_time, c_value = 0, 0
    logical_time, c_value  = local_event(pid, start_time, logical_time, c_value)
    logical_time, c_value = recv_event(pipe43, pid, get_physical_time(), logical_time, c_value)
    logical_time, c_value  = local_event(pid, get_physical_time(), logical_time, c_value)
    logical_time, c_value = send_event(pipe42, pid, get_physical_time(), logical_time, c_value)
    logical_time, c_value = recv_event(pipe43, pid, get_physical_time(), logical_time, c_value)
    logical_time, c_value = send_event(pipe42, pid, get_physical_time(), logical_time, c_value)


if __name__ == '__main__':
    one_and_two, two_and_one = Pipe()
    two_and_three, three_and_two = Pipe()
    three_and_four, four_and_three = Pipe()
    two_and_four, four_and_two = Pipe()

    start_time = get_physical_time()

    process1 = Process(target=process_one, 
                       args=(start_time, one_and_two, ))
    process2 = Process(target=process_two, 
                       args=(start_time, two_and_one, two_and_three, two_and_four))
    process3 = Process(target=process_three, 
                       args=(start_time, three_and_two, three_and_four))
    process4 = Process(target=process_four, 
                       args=(start_time, four_and_three, four_and_two))

    process1.start()
    process2.start()
    process3.start()
    process4.start()

    process1.join()
    process2.join()
    process3.join()
    process4.join()