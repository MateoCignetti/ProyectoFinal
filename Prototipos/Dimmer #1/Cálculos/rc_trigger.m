clear all, close all;

R = 83.3e3;
C = 470e-9;

num = 1;
den = [R*C 1];

H = tf(num,den);

figure();
%step(H)


t = [0:0.0001:160e-3];
u = 220*sqrt(2)*sin(2*pi*50*t);

[y, tOut] = lsim(H, u, t);

figure();
hold on;
plot(tOut,y);
%plot(t, u);

