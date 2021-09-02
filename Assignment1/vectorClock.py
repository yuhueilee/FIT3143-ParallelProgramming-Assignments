"""
Author: Vanessa Joyce Tan

Reference: https://towardsdatascience.com/understanding-lamport-timestamps-with-pythons-multiprocessing-library-12a6427881c6
"""
from multiprocessing import Process, Pipe
from typing import Set, Tuple

def vector_timestamp(partial_vt: Set[Tuple[int, int]], observed: int = None, event_vt: Set[Tuple[int, int]] = None):
    if event_vt == None:
        return '\tPARTIAL_VECTOR_CLOCK={}\n'.format(partial_vt)
    else:
        return '\tPARTIAL_VECTOR_CLOCK={}\n\tOBSERVED_EVENT_{}_TIMESTAMP={}\n'.format(partial_vt, observed, event_vt)

def event_observed(pid: int, partial_vt: Set[Tuple[int, int]], observed: int):
    event_vt = partial_vt
    new_observed = observed + 1
    partial_vt = set([(pid, new_observed)])
    
    print('process{}: An event has been observed!\n{}'
        .format(str(pid), vector_timestamp(partial_vt, observed, event_vt)))

    return partial_vt, new_observed

def send_event(pipe, pid: int, partial_vt: Set[Tuple[int, int]]):
    pipe.send((pid, partial_vt))

    print('process{} sends a message!\n{}'
        .format(str(pid), vector_timestamp(partial_vt)))

def recv_event(pipe, pid: int, partial_vt: Set[Tuple[int, int]]):
    sender, sender_partial_vt = pipe.recv()
    sender_partial_vt = set(sender_partial_vt)
    
    partial_vt_copy = partial_vt.copy()
    sender_partial_vt_copy = sender_partial_vt.copy()

    for tupA in partial_vt:
	    for tupB in sender_partial_vt:
		    if tupA[0] == tupB[0]:
			    if tupA < tupB:
				    partial_vt_copy.discard(tupA)
			    elif tupB < tupA:
				    sender_partial_vt_copy.discard(tupB)

    new_partial_vt = partial_vt_copy.union(sender_partial_vt_copy)
    
    print('process{} receives a message from process{}!\n{}'
        .format(str(pid), str(sender), vector_timestamp(new_partial_vt)))
    
    return new_partial_vt

def process_one(pipe12):
    pid = 1
    observed = 0
    partial_vt = set([(1, 0)])
    print('process{} Start!\n{}'
        .format(str(pid), vector_timestamp(partial_vt)))

    send_event(pipe12, pid, partial_vt)
    partial_vt = recv_event(pipe12, pid, partial_vt)
    partial_vt, observed = event_observed(pid, partial_vt, observed)
    send_event(pipe12, pid, partial_vt)

def process_two(pipe21, pipe23):
    pid = 2
    observed = 0
    partial_vt = set([(2, 0)])
    print('process{} Start!\n{}'
        .format(str(pid), vector_timestamp(partial_vt)))

    partial_vt = recv_event(pipe21, pid, partial_vt)
    send_event(pipe21, pid, partial_vt)
    send_event(pipe23, pid, partial_vt)
    partial_vt = recv_event(pipe21, pid, partial_vt)
    partial_vt = recv_event(pipe23, pid, partial_vt)
    partial_vt, observed = event_observed(pid, partial_vt, observed)

def process_three(pipe32):
    pid = 3
    observed = 0
    partial_vt = set([(3, 0)])
    print('process{} Start!\n{}'
        .format(str(pid), vector_timestamp(partial_vt)))

    partial_vt, observed = event_observed(pid, partial_vt, observed)
    partial_vt = recv_event(pipe32, pid, partial_vt)
    partial_vt, observed = event_observed(pid, partial_vt, observed)
    send_event(pipe32, pid, partial_vt)

if __name__ == '__main__':
    one_and_two, two_and_one = Pipe()
    two_and_three, three_and_two = Pipe()

    process1 = Process(target=process_one, 
                       args=(one_and_two,))
    process2 = Process(target=process_two, 
                       args=(two_and_one, two_and_three))
    process3 = Process(target=process_three, 
                       args=(three_and_two,))

    process1.start()
    process2.start()
    process3.start()

    process1.join()
    process2.join()
    process3.join()