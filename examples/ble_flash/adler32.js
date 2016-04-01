exports.adler32_buf = function(buf, init) {
    var a = 1, b = 0, L = buf.length, M;

    if (init)
    {
        a = init & 0xffff;
        b = init & 0xffff >> 16;
    }

    for(var i = 0; i < L;) {
        M = Math.min(L-i, 3850)+i;
        for(;i<M;i++) {
            a += buf[i];
            b += a;
        }
        a = (15*(a>>>16)+(a&65535));
        b = (15*(b>>>16)+(b&65535));
    }
    return ((b%65521) << 16) | (a%65521);
}