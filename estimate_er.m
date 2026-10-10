clear;
clc;

% Material parameters
eps0 = 8.8541878128e-12;
c0   = 299792458;

n       = 3.59;
eps_r   = n^2;
T2      = 0.07e-12;    % Replace with exact simulation value
omega0  = 2*pi*337e12; % Material resonance, not necessarily cavity resonance
sigma0  = -5760;       % Conductivity parameter, S/m
s       = 1.0;         % Unsaturated response

freq = [337e12, 383e12];
omega = 2*pi*freq;

omega = 337e12*2*pi;
%omega = 383e12*2*pi;

alpha = -668000;
%alpha = -1040216;

A = (1 + (omega0^2 + omega^2)*T2^2) / ((1+(omega0^2 - omega^2)*T2^2)^2 + 4*omega^2*T2^2);
B = (omega * T2 * (-1 + (omega0^2 - omega^2)*T2^2)) / ((1 + ((omega0^2 - omega^2)*T2^2))^2 + 4 * omega^2 * T2^2);

a = A^2;
b = (B * alpha^2 * 4 * c0^2 * eps0) / omega;
c = -alpha^2 * 4 * c0^2 * eps0^2 * eps_r;

r = roots([a, b, c]);
disp(r);