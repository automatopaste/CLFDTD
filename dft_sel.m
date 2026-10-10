clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

c0 = 299792458;          % m/s

% FDTD sampling
dx0 = 2.033e-9;          % m
courant = 0.5;
dt = courant * dx0 / c0;

% Medium
n = 3.59;

% Cavity length
L = 1.2395543175e-5;     % m

% Reference wavelength
lambdaB = 870e-9;        % m

% E-field component: 1 = Ex, 2 = Ey, 3 = Ez
component = 3;

% Desired centre frequency
omega0 = 2 * pi * c0 / lambdaB;

% Desired total angular-frequency width
omegaWidth = 1.00e15;

% Frequency limits
omegaMin = omega0 - omegaWidth/2;
omegaMax = omega0 + omegaWidth/2;

% Additional zero samples added symmetrically to files
nZeros = 0;

% Incident-spectrum validity threshold, relative to peak power
incidentThreshold = 1e-8;

% ------------------------------------------------------------
% Resonance plotting
% ------------------------------------------------------------

mCentral = 100;
modeRange = 12;

%% ============================================================
% Select incident reference file
%
% This file should be from a simulation WITHOUT the cavity/DBRs.
% It records the incident pulse at the same monitor location.
% ============================================================

[fileInc, pathInc] = uigetfile('*.*', ...
'Select INCIDENT REFERENCE file (no cavity)');

if isequal(fileInc, 0)
return;
end

fid = fopen(fullfile(pathInc, fileInc), 'rb');

if fid == -1
error('Could not open incident reference file.');
end

dataInc = fread(fid, 'single');
fclose(fid);

if mod(numel(dataInc), 3) ~= 0
error('Incident file does not contain whole vec3 samples.');
end

dataInc = reshape(dataInc, 3, []);
Einc_original = double(dataInc(component, :).');

%% ============================================================
% Select total-field file
%
% This file should be from a simulation WITH the passive cavity.
% It records incident + reflected fields at the same monitor.
% ============================================================

[fileTotal, pathTotal] = uigetfile('*.*', ...
'Select TOTAL FIELD file (with passive cavity)');

if isequal(fileTotal, 0)
return;
end

fid = fopen(fullfile(pathTotal, fileTotal), 'rb');

if fid == -1
error('Could not open total-field file.');
end

dataTotal = fread(fid, 'single');
fclose(fid);

if mod(numel(dataTotal), 3) ~= 0
error('Total-field file does not contain whole vec3 samples.');
end

dataTotal = reshape(dataTotal, 3, []);
Etotal_original = double(dataTotal(component, :).');

%% ============================================================
% Check and prepare signals
% ============================================================

if numel(Einc_original) ~= numel(Etotal_original)
error(['The two files have different sample counts. ' ...
'Use matching simulation durations and sampling rates.']);
end

% Add identical symmetric zero padding
Einc = [zeros(nZeros,1); Einc_original; zeros(nZeros,1)];
Etotal = [zeros(nZeros,1); Etotal_original; zeros(nZeros,1)];

N = numel(Einc);
time = (0:N-1).' * dt;

% Remove each signal's DC offset
Einc = Einc - mean(Einc);
Etotal = Etotal - mean(Etotal);

% Isolate reflected field by subtracting the incident reference
Eref = Etotal - Einc;

fprintf('\n');
fprintf('============================================================\n');
fprintf('FDTD sampling information\n');
fprintf('============================================================\n');
fprintf('Incident file       = %s\n', fileInc);
fprintf('Total-field file    = %s\n', fileTotal);
fprintf('dx0                 = %.12e m\n', dx0);
fprintf('dt                  = %.12e s\n', dt);
fprintf('dt                  = %.6f fs\n', dt*1e15);
fprintf('Number of samples   = %d\n', N);
fprintf('Total time          = %.6f ps\n', time(end)*1e12);
fprintf('Sampling frequency  = %.6f PHz\n', 1/dt/1e15);
fprintf('Nyquist frequency   = %.6f PHz\n', 1/(2*dt)/1e15);
fprintf('============================================================\n\n');

%% ============================================================
% FFT of incident, total and reflected fields
% ============================================================

Einc_fft = fft(Einc);
Etotal_fft = fft(Etotal);
Eref_fft = fft(Eref);

% Positive-frequency spectrum
Nhalf = floor(N/2) + 1;

f = (0:Nhalf-1).' / (N * dt);
omega = 2*pi*f;

Ein = Einc_fft(1:Nhalf);
Etot = Etotal_fft(1:Nhalf);
Er = Eref_fft(1:Nhalf);

% Incident spectral power
incidentPower = abs(Ein).^2;

% Reject frequencies with negligible incident energy
validIncident = incidentPower > ...
max(incidentPower) * incidentThreshold;

% Reflectivity: reflected power / incident power
R = nan(size(omega));
R(validIncident) = abs(Er(validIncident)).^2 ./ ...
incidentPower(validIncident);

% Field-amplitude reflection coefficient magnitude
rMagnitude = nan(size(omega));
rMagnitude(validIncident) = abs(Er(validIncident)) ./ ...
abs(Ein(validIncident));

% Total-field spectrum normalised for plotting
totalMag = abs(Etot);
totalMagNorm = totalMag / max(totalMag);

%% ============================================================
% Select desired angular-frequency range
% ============================================================

idxOmega = omega >= omegaMin & omega <= omegaMax;

omegaPlot = omega(idxOmega);
lambdaPlot = 2*pi*c0 ./ omegaPlot;

RPlot = R(idxOmega);
rPlot = rMagnitude(idxOmega);
totalMagPlot = totalMagNorm(idxOmega);

validPlot = isfinite(RPlot);

% Restrict wavelength to requested frequency interval
lambdaMin = 2*pi*c0 / omegaMax;
lambdaMax = 2*pi*c0 / omegaMin;

%% ============================================================
% Fabry-Perot resonant modes
%
% lambda_m = 2*n*L/m
% ============================================================

mValues = (mCentral - modeRange):(mCentral + modeRange);

lambdaModes = 2*n*L ./ mValues;
omegaModes = 2*pi*c0 ./ lambdaModes;

validModes = omegaModes >= omegaMin & omegaModes <= omegaMax;

mValues = mValues(validModes);
lambdaModes = lambdaModes(validModes);
omegaModes = omegaModes(validModes);

mAtlambdaB = 2*n*L / lambdaB;
lambdaAtM100 = 2*n*L / mCentral;
omegaAtM100 = 2*pi*c0 / lambdaAtM100;

fprintf('\n');
fprintf('============================================================\n');
fprintf('Fabry-Perot cavity information\n');
fprintf('============================================================\n');
fprintf('Cavity length              = %.6f um\n', L*1e6);
fprintf('Refractive index           = %.6f\n', n);
fprintf('Reference wavelength       = %.3f nm\n', lambdaB*1e9);
fprintf('m corresponding to lambdaB = %.6f\n', mAtlambdaB);
fprintf('m = %d wavelength           = %.6f nm\n', ...
mCentral, lambdaAtM100*1e9);
fprintf('m = %d angular frequency    = %.6e rad/s\n', ...
mCentral, omegaAtM100);

fprintf('\nResonant modes:\n');
fprintf('------------------------------------------------------------\n');
fprintf(' m        lambda (nm)          omega (rad/s)\n');
fprintf('------------------------------------------------------------\n');

for k = 1:numel(mValues)
fprintf('%3d      %12.6f       %15.6e\n', ...
mValues(k), lambdaModes(k)*1e9, omegaModes(k));
end

fprintf('============================================================\n\n');

%% ============================================================
% Plot time-domain fields
% ============================================================

timeWindow_ps = 7.0;
idxTimeZoom = time*1e12 <= timeWindow_ps;

figure('Color', 'w');

plot(time(idxTimeZoom)*1e12, ...
Einc(idxTimeZoom)/1000, ...
'LineWidth', 1.0);
hold on;

plot(time(idxTimeZoom)*1e12, ...
Etotal(idxTimeZoom)/1000, ...
'LineWidth', 1.0);

plot(time(idxTimeZoom)*1e12, ...
Eref(idxTimeZoom)/1000, ...
'LineWidth', 1.0);

hold off;
xlabel('Time (ps)');
ylabel(sprintf('E_%d (kV/m)', component));
title(sprintf('Incident, total and reflected fields — first %.2f ps', ...
timeWindow_ps));
legend('Incident reference', 'Total field', 'Reflected field');
grid on;

%% ============================================================
% Plot reflectivity versus wavelength
% ============================================================

figure('Color', 'w');

plot(lambdaPlot(validPlot)*1e9, RPlot(validPlot), ...
'LineWidth', 1.2);

xlabel('Wavelength (nm)');
ylabel('Power reflectivity R');
title('Passive cavity reflectivity spectrum');
xlim([lambdaMin lambdaMax]*1e9);
grid on;

xline(lambdaB*1e9, '--', 'Bragg wavelength');

%% ============================================================
% Plot reflectivity versus frequency
% ============================================================

figure('Color', 'w');

plot(omegaPlot(validPlot)/(2*pi*1e12), RPlot(validPlot), ...
'LineWidth', 1.2);

xlabel('Frequency (THz)');
ylabel('Power reflectivity R');
title('Passive cavity reflectivity spectrum');
grid on;

xline(c0/lambdaB/1e12, '--', 'Bragg frequency');

%% ============================================================
% Plot normalised total-field FFT versus wavelength
% ============================================================

figure('Color', 'w');

plot(lambdaPlot*1e9, totalMagPlot, 'LineWidth', 1.2);

xlabel('Wavelength (nm)');
ylabel('Normalised |FFT|');
title('Total-field FFT spectrum');
grid on;
xlim([lambdaMin lambdaMax]*1e9);

%% ============================================================
% Calculate theoretical free spectral range
% ============================================================

fFSR = c0 / (2*n*L);
omegaFSR = 2*pi*fFSR;

fprintf('Theoretical Fabry-Perot FSR:\n');
fprintf('  Delta f     = %.6f THz\n', fFSR/1e12);
fprintf('  Delta omega = %.6e rad/s\n', omegaFSR);
fprintf('  T_RT        = %.6f ps\n', 1/fFSR*1e12);

fprintf('\nReflectivity summary:\n');
fprintf('  Incident spectrum threshold = %.1e of peak power\n', ...
incidentThreshold);
fprintf('  Valid frequency bins        = %d / %d in requested range\n', ...
nnz(validPlot), numel(validPlot));

if any(validPlot)
fprintf('  Maximum computed R in range = %.6f\n', ...
max(RPlot(validPlot)));
end

fprintf('\n');
