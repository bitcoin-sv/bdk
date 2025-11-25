void C_NoOp() {
    // This is the boundary landing spot in C. It performs zero work.
    // We are measuring the Go runtime's actions (scheduler coordination,
    // stack switch, register save/restore) required to enter and exit this function.
}

long long C_SumBytes(const unsigned char *data, int len) {
    // Sum all bytes in the array
    // This measures CGO overhead + data pointer passing overhead
    long long sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}
