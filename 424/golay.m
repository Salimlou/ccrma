function [responses] = golay ()
a = [1];
b = [1];
n = 10;
while n >= 0
    n = n - 1;
    aNext = [a b];
    bNext = [a b*-1];
    a = aNext;
    b = bNext;
    
end
g = xcorr(a, a) + xcorr(b, b);