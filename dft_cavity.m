clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

c0 = 299792458;          % m/s

% FDTD sampling
dx0 = 1.2395543175e-5 / 5600;  % m
courant = 0.5;
dt = courant * dx0 / c0;

% Medium
n = 3.59;

% Cavity length
L = 1.2395543175e-5;             % m

% Reference wavelength
lambda0 = 890e-9;        % m

% E-field component
% 1 = Ex
% 2 = Ey
% 3 = Ez
component = 3;

% Desired centre frequency
omega0 = 2 * pi * c0 / lambda0;        % rad/s

% Desired total angular-frequency width
omegaWidth = 0.60e15;    % rad/s

% Frequency limits
omegaMin = omega0 - omegaWidth/2;
omegaMax = omega0 + omegaWidth/2;

% Additional zero samples added symmetrically to files
nZeros = 0;

% ------------------------------------------------------------
% Resonance plotting
% ------------------------------------------------------------

% Central longitudinal mode
mCentral = 100;

% Number of modes to show on either side of mCentral
modeRange = 12;


%% ============================================================
% Select file
% ============================================================

[file1, path1] = uigetfile('*.*', 'Select vec3 file');

if isequal(file1, 0)
    return;
end

fid = fopen(fullfile(path1, file1), 'rb');

if fid == -1
    error('Could not open first file.');
end

data1 = fread(fid, 'single');
fclose(fid);

if mod(numel(data1), 3) ~= 0
    error('First file does not contain a whole number of vec3 samples.');
end

data1 = reshape(data1, 3, []);

% Extract selected field component
E1_original = double(data1(component, :)).';

% Time-domain signal including the symmetric zero padding
E1_time = [zeros(nZeros, 1); E1_original; zeros(nZeros, 1)];


%% ============================================================
% Time vector
% ============================================================

N = numel(E1_time);

time = (0:N-1).' * dt;

fprintf('\n');
fprintf('============================================================\n');
fprintf('FDTD sampling information\n');
fprintf('============================================================\n');
fprintf('dx0                 = %.12e m\n', dx0);
fprintf('dt                  = %.12e s\n', dt);
fprintf('dt                  = %.6f fs\n', dt*1e15);
fprintf('Number of samples   = %d\n', N);
fprintf('Total time          = %.6f ps\n', time(end)*1e12);
fprintf('Sampling frequency  = %.6f PHz\n', 1/dt/1e15);
fprintf('Nyquist frequency   = %.6f PHz\n', 1/(2*dt)/1e15);
fprintf('============================================================\n\n');


%% ============================================================
% FFT
% ============================================================

% Make a separate copy for FFT processing
E1 = E1_time;

% Remove DC component
E1 = E1 - mean(E1);

% FFT
E1_fft = fft(E1);

% Frequency vector
f = (0:N-1).' / (N * dt);

% Angular frequency
omega = 2*pi*f;

% Single-sided spectrum
Nhalf = floor(N/2) + 1;

omega = omega(1:Nhalf);
f = f(1:Nhalf);

% FFT magnitude
mag = abs(E1_fft(1:Nhalf));

% Normalise only for plotting
magNorm = mag / max(mag);


%% ============================================================
% Select desired angular-frequency range
% ============================================================

idxOmega = ...
    omega >= omegaMin & ...
    omega <= omegaMax;

omegaPlot = omega(idxOmega);
magPlot = magNorm(idxOmega);

% Corresponding wavelength
lambdaPlot = 2*pi*c0 ./ omegaPlot;


%% ============================================================
% Fabry-Perot resonant modes
%
% lambda_m = 2*n*L/m
%
% mCentral = 100
% ============================================================

mValues = ...
    (mCentral - modeRange): ...
    (mCentral + modeRange);

lambdaModes = 2*n*L ./ mValues;

omegaModes = 2*pi*c0 ./ lambdaModes;

% Only retain resonances inside requested spectral range
validModes = ...
    omegaModes >= omegaMin & ...
    omegaModes <= omegaMax;

mValues = mValues(validModes);
lambdaModes = lambdaModes(validModes);
omegaModes = omegaModes(validModes);


%% ============================================================
% Actual mode number corresponding to lambda0
% ============================================================

mAtLambda0 = 2*n*L / lambda0;

lambdaAtM100 = 2*n*L / mCentral;

omegaAtM100 = 2*pi*c0 / lambdaAtM100;

fprintf('\n');
fprintf('============================================================\n');
fprintf('Fabry-Perot cavity information\n');
fprintf('============================================================\n');
fprintf('Cavity length              = %.6f um\n', L*1e6);
fprintf('Refractive index           = %.6f\n', n);
fprintf('Reference wavelength       = %.3f nm\n', lambda0*1e9);
fprintf('m corresponding to lambda0 = %.6f\n', mAtLambda0);
fprintf('\n');

fprintf('m = %d wavelength           = %.6f nm\n', ...
    mCentral, lambdaAtM100*1e9);

fprintf('m = %d angular frequency    = %.6e rad/s\n', ...
    mCentral, omegaAtM100);

fprintf('\n');

fprintf('Resonant modes:\n');
fprintf('------------------------------------------------------------\n');
fprintf(' m        lambda (nm)          omega (rad/s)\n');
fprintf('------------------------------------------------------------\n');

for k = 1:numel(mValues)

    fprintf('%3d      %12.6f       %15.6e\n', ...
        mValues(k), ...
        lambdaModes(k)*1e9, ...
        omegaModes(k));

end

fprintf('============================================================\n\n');


%% ============================================================
% Plot TIME-DOMAIN field — first portion / pulse detail
% ============================================================
%
% This is useful for inspecting the actual optical waveform.
% The displayed interval can be changed below.

timeWindow_ps = 7.0;

figure('Color', 'w');

idxTimeZoom = time*1e12 <= timeWindow_ps;

plot(time(idxTimeZoom)*1e12, ...
     E1_time(idxTimeZoom) / 1000, ...
     'LineWidth', 1.0);

xlabel('Time (ps)');
ylabel(sprintf('E_z (kV/m)'));

title(sprintf( ...
    'Time-Domain Field — First %.2f ps', ...
    timeWindow_ps));

ylim([-1200, 1200]);

grid on;


%% ============================================================
% Plot FFT versus angular frequency
% ============================================================

figure('Color', 'w');

plot(omegaPlot, magPlot, 'LineWidth', 1.2);
hold on;

% Vertical resonance lines
for k = 1:numel(mValues)

    xline(omegaModes(k), '--', ...
        sprintf('m=%d', mValues(k)), ...
        'LabelVerticalAlignment', 'middle', ...
        'LabelOrientation', 'horizontal', ...
        'HandleVisibility', 'off');

end

% Central reference frequency
xline(omega0, 'k-', ...
    '\lambda_0 = 890 nm', ...
    'LineWidth', 1.2, ...
    'LabelVerticalAlignment', 'bottom', ...
    'HandleVisibility', 'off');

hold off;

xlabel('\omega (rad/s)');
ylabel('Normalised |FFT|');

title('Vec3 FFT Spectrum — Angular Frequency');

grid on;
xlim([omegaMin omegaMax]);


%% ============================================================
% Plot FFT versus wavelength
%
% Wavelength axis is reversed so that wavelength increases
% from right to left, matching the conventional spectrum view.
% ============================================================

figure('Color', 'w');

plot(lambdaPlot*1e9, magPlot, 'LineWidth', 1.2);
hold on;

for k = 1:numel(mValues)

    xline(lambdaModes(k)*1e9, '--', ...
        sprintf('m=%d', mValues(k)), ...
        'LabelVerticalAlignment', 'middle', ...
        'LabelOrientation', 'horizontal', ...
        'HandleVisibility', 'off');

end

% Central wavelength
xline(lambda0*1e9, 'k-', ...
    '\lambda_0 = 890 nm', ...
    'LineWidth', 1.2, ...
    'LabelVerticalAlignment', 'bottom', ...
    'HandleVisibility', 'off');

hold off;

xlabel('\lambda (nm)');
ylabel('Normalised |FFT|');

title('Vec3 FFT Spectrum — Wavelength');

grid on;

% Restrict wavelength to equivalent angular-frequency interval
lambdaMin = 2*pi*c0 / omegaMax;
lambdaMax = 2*pi*c0 / omegaMin;

xlim([lambdaMin lambdaMax]*1e9);

% Conventional optical-spectrum orientation
set(gca, 'XDir', 'reverse');


%% ============================================================
% Optional: zoom around the 890 nm region
% ============================================================

figure('Color', 'w');

plot(lambdaPlot*1e9, magPlot, 'LineWidth', 1.2);
hold on;

for k = 1:numel(mValues)

    xline(lambdaModes(k)*1e9, '--', ...
        sprintf('m=%d', mValues(k)), ...
        'HandleVisibility', 'off');

end

hold off;

xlabel('\lambda (nm)');
ylabel('Normalised |FFT|');

title('Fabry-Perot Modes Around 890 nm');

grid on;

% Narrow zoom around target
xlim([lambda0 - 10e-9, lambda0 + 10e-9]*1e9);

set(gca, 'XDir', 'reverse');


%% ============================================================
% Calculate theoretical free spectral range
% ============================================================

fFSR = c0 / (2*n*L);
omegaFSR = 2*pi*fFSR;

fprintf('Theoretical Fabry-Perot FSR:\n');
fprintf('  Δf      = %.6f THz\n', fFSR/1e12);
fprintf('  Δomega  = %.6e rad/s\n', omegaFSR);
fprintf('  T_RT    = %.6f ps\n', 1/fFSR*1e12);
fprintf('\n');

fprintf('Observed 0.37 ps period corresponds to:\n');
fprintf('  Δf      = %.6f THz\n', 1/(0.37e-12)/1e12);
fprintf('  Δomega  = %.6e rad/s\n', 2*pi/(0.37e-12));
fprintf('\n');
