# Generic Virtual Platform Testing Interface

This is an abstract definition of a generic testing interface for virtual platforms. This can be used by a client to send requests via the implemented communication methods (currently message queues and pipes) to control the simulation of a virtual platform. There are many different commands available, that concentrate on doing tests (for example fuzzing) on the target software, that is running inside the VP. There is also a special MMIO (Memory Mapped IO) interception concept implemented, which can enhance the testing process and possibilities. More information about this concept can be found here: <mark>TODO</mark>.

[Here](##Commands) you can find the list of available commands.

## Implementations

### Virtual Platforms

Here is a list of virtual platforms that implemented this testing interface. More information on how the interface can be enabled and configured, please check out the usage information.

|Virtual Platform|Description|Version|Repository|Usage Information|
|---|---|---|---|---|
|AVP64|SystemC based ARMv8 Virtual Platform for 64bit Cortex-A cores.|v0.2|[testing-interface branch of fork by Jonaswinz](https://github.com/Jonaswinz/avp64/tree/testing-interface)|<mark>TODO</mark>|
|AVP64 (32 bit version)|Modified version of AVP64 with Cortex-M cores.|v0.2|<mark>TODO</mark>|<mark>TODO</mark>|

### Clients

The client can be any program that is able to send the requests (commands and data) via an implemented testing communication (currently MQ and pipes). Here is a list of tools, with this communication implemented. More information on how to use the client / tool can be found under usage information. 

|Client|Description|Version|Repository|Usage Information|
|---|---|---|---|---|
|AFLplsuplus|v0.2|Coverage guided fuzzing. The VP testing interface can be used with the -V mode. The harness (which does the communication between AFLplusplus and the VP) uses pipes.|[vp-mode branch of fork by Jonaswinz](https://github.com/Jonaswinz/AFLplusplus/tree/vp-mode)|<mark>TODO</mark>|


## Available Commands

A command and its data are sent as a request to the VP and a response will be sent back after its execution. The handling is always sequential, so one request at a time. The response may contain data (depending on the command) and always contains status indicator, which can be STATUS_OK, STATUS_ERROR, STATUS_MALFORMED. If the status is STATUS_MALFORMED, then the sent data is not valid. The format of the request and response depends on the selected communication.

|Command|Desciption|Data|Return|
|---|---|---|---|
|CONTINUE|Continues the simulation to the next event and returns this event (BREAKPOINT_HIT, MMIO_READ, MMIO_WRITE, VP_FINISHED). When multiple event occurred, then this command will return the first element in the queue and removes it. If the event queue is not empty after the CONTINUE request, the simulation will not continue. In this case, the CONTINUE command needs to be called multiple times until all events are processed. If a MMIO_READ or MMIO_WRITE event occurred and CONTINUE is requested (afterward), there will not be a read / write from the CPU perspective (because it was intercepted). If data should be written / read, the SET_MMIO_VALUE need to be sent before CONTINUE.|None|**Byte 0**: First event of the event queue, <br/>**For MMIO_WRITE**: Byte 1-?: Symbol name. <br/>**For MMIO_READ**: Byte 1-8: MMIO address, Byte 9-16: length. <br/>**For MMIO_WRITE**: Byte 1-8: MMIO address, Byte 9-16: length, Byte 17-?: MMIO data.|
|KILL|Kills the simulation and the whole VP process. If gracefully is set to 1, then the VP waits for the simulation to finish before killing itself. During this time other event may occur and if the target program never finishes, then the VP will wait infinitely.|**Byte 0**: Gracefully (0/1)| None|
|SET_BREAKPOINT|Sets a breakpoint to the given symbol name and offset. If the breakpoint is reached, the continue function will return the event BREAKPOINT_HIT. The breakpoint will be removed after the event, and thus may need to be set again (if needed).|**Byte 0**: Offset (uint8), <br/>**Byte 1-?**: Symbol name (string)|None|
|REMOVE_BREAKPOINT|Removes a breakpoint by its symbol name.|**Byte 0-?**: Symbol name (string)|None|
|ENABLE_MMIO_TRACKING|Enables MMIO tracking/interception.|None|None|
|DISABLE_MMIO_TRACKING|Disables MMIO tracking/interception.|None|None|
|SET_MMIO_READ|Sets the value after an MMIO_READ or MMIO_WRITE event. When running CONTINUE after this command the set data will then be injected into the bus read/write request. The length must be the same as the read/write event that was intercepted. The return of the CONTINUE command that indicated the MMIO_READ or MMIO_WRITE event contains the length information.|None|None|
|ADD_TO_MMIO_READ_QUEUE|Adds elements to the MMIO read queue, which means, that if the CPU requests reads that fit to one or multiple elements in the read queue (and MMIO tracking is enabled for the requested range) it will not suspend the simulation and trigger a MMIO_READ event but rather directly use the value. One element consists of an address, length and value. The element will be used if the address and length matches with the read request of the CPU. Every element will only be used once. If there are multiple elements that fit, the first one will be taken. With this command, multiple elements can be sent at once (successively). The value length then needs to fit the element count and element length.|**Byte 0-7**: Address (uint64), <br/>**Byte 8-11**: Length (uint32) <br/>**Byte 12-15**: Element count (uint32), <br/>**Byte 16-?**: Value for all elements|None|
|TRIGGER_CPU_INTERRUPT|Triggers a CPU interrupt manually by its ID.|**Byte 0**: ID of the interrupt (uint8)|None|
|ENABLE_CODE_COVERAGE|Enables code coverage tracking.|None|None|
|DISABLE_CODE_COVERAGE|Disables code coverage tracking.|None|None|
|GET_CODE_COVERAGE|Reads the current code coverage.|None|**Byte 0-?**: Code coverage (string)|
|GET_CODE_COVERAGE_SHM|Writes the current code coverage to a shared memory region.|**Byte 0-3**: Shared memory ID (uint32), <br/>**Byte 4-7**: Write offset (uint32)|None|
|RESET_CODE_COVERAGE|Resets the code coverage, by writing zeros to the array.|None|None|
|SET_RETURN_CODE_ADDRESS|Sets the address of the code, where the return code should be recorded, by reading the given register.|**Byte 0-7**: Address of the instruction (uint64), <br/>**Byte 8-?**: Name of the register (string)|None|
|GET_RETURN_CODE|Reads the captured return code, specified by SET_RETURN_CODE_ADDRESS. If the return code was not captured, it will through an error.|None|**Byte 0-7**: Return code (uint64)|
|DO_RUN|This command triggers one "run" from a start symbol to an end symbol with one or multiple read elements. This effectively is a combination of SET_BREAKPOINT and ADD_TO_MMIO_READ_QUEUE, but executes much faster, because it is doing everything at once. Also, all other events are ignored during this time!|**Byte 0-7**: Address (uint64), <br/>**Byte 8-11**: Length (uint32) <br/>**Byte 12-15**: Element count (uint32), <br/>**Byte 16**: Start breakpoint name length, <br/>**Byte 17**: End breakpoint name length, <br/>**Byte 18-?**: Start breakpoint symbol name, <br/>**Byte ?-?**: End breakpoint symbol name, <br/>**Byte ?-?**: Value for all elements|None|
|DO_RUN_SHM|Does the same as DO_RUN, but takes the MMIO elements from a shared memory region. The shared memory region will be cut into parts according to element length. If the shared memory region length is not a multiple of the element length, STATUS_ERROR will be returned. Additionally, an option can be settled to stop after the string termination character when reading the shared memory region, to not have many zero elements, when the shared memory size is larger than the wanted MMIO data.|**Byte 0-7**: Address (uint64), <br/>**Byte 8-11**: Length (uint32), <br/>**Byte 12-15**: Shared memory ID, <br/>**Byte 16-19**: Write offset (uint32), <br/>**Byte 20**: Option: "stop after string termination", <br/>**Byte 21**: Start breakpoint name length, <br/>**Byte 22**: End breakpoint name length, <br/>**Byte 23-?**: Start breakpoint symbol name, <br/>**Byte ?-?**: End breakpoint symbol name|None|


## New Client

Implementation of a client is quite easy. Just use the testing_client class to send the requests and parse responses via the wanted communication interface. Inside the `test/client/` folder, you find examples on how to use it. The client should be always started before the VP, because it creates the message queues / pipes if not exist and clears lost data. When using message queues, only MQ_MAX_LENGTH (default 256) - 1 bytes of data is supported for the request. The reponse is not limited.

## New VP Implementation

This project contains the abstract classes `testing_receiver` and `testing_communication`. To use the testing interface, both classes must be implemented for the concrete VP and communication. The `testing_receiver` handles the received requests and calls the corresponding (abstract) handler methods. The class `testing_communication` does the communication (request receiving and sending). It is already implemented for pipes (`pipe_testing_communication`) and message queues (`mq_testing_communication`). In order to add the VP testing interface to a new VP, the `testing_receiver` class need to be implemented. And if a different communication (other than MQ and pipes) is required, then also another version of `testing_communication` needs to be created. Please take a look at the example inside the `test/implementation` folder. It is maybe also a good idea to take a look at the current VP implementations. For example, `avp64_testing_receiver` class of AVP64. This project does not define much of the actual implementations of the different commands (to have flexibility when doing the implementations). When implementing a new VP, please implement the command handlers with the same functionality as defined in this README.

This diagram shows the relations between the classes and all virtual functions. The virtual functions are additionally highlighted.

![plot](./assets/vp-testing-interface.drawio.svg)


## Improvements / Future Ideas:
- Client library for communication
- CPU interrupt event
- Support for multiple different MMIO probes
- Handle multiple request at once: faster performance for specific cases.