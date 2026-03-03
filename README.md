# SizedRingBuffer
A constant capacity ring buffer storing dynamic size.
Significantly faster (~2.5/1.2 times faster push/pop respectively) than std::vector.
Overcapacity pushes overwrite oldest elements.
