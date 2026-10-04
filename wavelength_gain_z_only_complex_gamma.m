clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

% Speed of light
c0 = 299792458;             % m/s

% FDTD temporal sampling interval
dx0 = 6.2e-10;
courant = 0.5;
dt = courant * dx0 / c0;    % seconds

% Angular frequency to center the plot around
centerFrequency = 3.37e14;                  % Hz
centerOmega = 2*pi*centerFrequency;         % rad/s

% Angular-frequency range displayed around center
omegaWidth = 0.6e15;                        % rad/s

% Apply Hann window before FFT
useWindow = true;

%% ============================================================
% Select files
% ============================================================

[file1, path1] = uigetfile('*.*', ...
    'Select reference E-field file');

if isequal(file1, 0)
    return;
end

[file2, path2] = uigetfile('*.*', ...
    'Select amplified E-field file');

if isequal(file2, 0)
    return;
end

%% ============================================================
% Load Vec3 float32 files
% ============================================================

fid = fopen(fullfile(path1, file1), 'rb');
data1 = fread(fid, inf, 'float32=>single');
fclose(fid);

fid = fopen(fullfile(path2, file2), 'rb');
data2 = fread(fid, inf, 'float32=>single');
fclose(fid);

% Number of Vec3 samples
N1 = floor(length(data1) / 3);
N2 = floor(length(data2) / 3);

% Discard incomplete final Vec3 if necessary
data1 = data1(1 : 3*N1);
data2 = data2(1 : 3*N2);

% Convert:
%
% [x0 y0 z0 x1 y1 z1 ...]
%
% into:
%
% [x0 y0 z0
%  x1 y1 z1
%  ...]

E1 = reshape(data1, 3, N1).';
E2 = reshape(data2, 3, N2).';

fprintf('File 1: %d Vec3 samples\n', N1);
fprintf('File 2: %d Vec3 samples\n', N2);

%% ============================================================
% Use common length
% ============================================================

N = min(N1, N2);

E1 = double(E1(1:N, :));
E2 = double(E2(1:N, :));

%% ============================================================
% Remove DC from each component
% ============================================================

E1 = E1 - mean(E1, 1);
E2 = E2 - mean(E2, 1);

%% ============================================================
% Window
% ============================================================

if useWindow
    window = hann(N);

    E1 = E1 .* window;
    E2 = E2 .* window;
end

%% ============================================================
% FFT zero padding
% ============================================================

% Desired number of plotted FFT samples across the displayed
% angular-frequency window.
desiredBinsAcrossWidth = 1000;

% Required angular-frequency sample spacing.
desiredDeltaOmega = omegaWidth / desiredBinsAcrossWidth;

% Required FFT size.
requiredNfft = ceil(2*pi / (dt * desiredDeltaOmega));

% Round up to a power of two.
Nfft = 2^nextpow2(max(N, requiredNfft));

fprintf('\nFFT information:\n');
fprintf('  Original samples       : %d\n', N);
fprintf('  FFT samples             : %d\n', Nfft);
fprintf('  Time duration           : %.6e s\n', N * dt);

trueDeltaOmega = 2*pi / (N * dt);
fftDeltaOmega = 2*pi / (Nfft * dt);

fprintf('  True angular resolution: %.6e rad/s\n', trueDeltaOmega);
fprintf('  FFT bin spacing         : %.6e rad/s\n', fftDeltaOmega);
fprintf('  Plot angular width      : %.6e rad/s\n', omegaWidth);
fprintf('  FFT bins across width   : %.1f\n', ...
    omegaWidth / fftDeltaOmega);

%% ============================================================
% FFT each Cartesian component
% ============================================================

Fx1 = fft(E1(:,1), Nfft);
Fy1 = fft(E1(:,2), Nfft);
Fz1 = fft(E1(:,3), Nfft);

Fx2 = fft(E2(:,1), Nfft);
Fy2 = fft(E2(:,2), Nfft);
Fz2 = fft(E2(:,3), Nfft);

%% ============================================================
% Preserve full FFTs for comparison plots
% ============================================================

Fx1_full = Fx1;
Fy1_full = Fy1;
Fz1_full = Fz1;

Fx2_full = Fx2;
Fy2_full = Fy2;
Fz2_full = Fz2;

%% ============================================================
% Single-sided spectrum
% ============================================================

nHalf = floor(Nfft/2) + 1;

Fx1 = Fx1_full(1:nHalf);
Fy1 = Fy1_full(1:nHalf);
Fz1 = Fz1_full(1:nHalf);

Fx2 = Fx2_full(1:nHalf);
Fy2 = Fy2_full(1:nHalf);
Fz2 = Fz2_full(1:nHalf);

Fs = 1 / dt;

% FFT frequency in Hz
frequency = (0:nHalf-1).' * Fs / Nfft;

% Convert to angular frequency
omega = 2*pi*frequency;

%% ============================================================
% Select angular-frequency range
% ============================================================

omegaMin = centerOmega - omegaWidth / 2;
omegaMax = centerOmega + omegaWidth / 2;

maskFFT = omega >= omegaMin & omega <= omegaMax;

omegaFFT = omega(maskFFT);

% Z-component magnitude spectrum only
magnitude1 = abs(Fz1(maskFFT));
magnitude2 = abs(Fz2(maskFFT));

%% ============================================================
% Plot individual FFTs for visual comparison
% ============================================================

figure('Name', 'Z-Component FFT Comparison', ...
       'Color', 'w');

plot(omegaFFT, magnitude1, 'LineWidth', 1.2);
hold on;
plot(omegaFFT, magnitude2, 'LineWidth', 1.2);
hold off;

xlabel('\omega (rad/s)');
ylabel('|FFT(E_z)|');
title('E_z FFT');

legend('Reference', 'Amplified', 'Location', 'best');
grid on;
xlim([omegaMin omegaMax]);
xline(centerOmega, '--');

%% ============================================================
% Plot individual FFTs on logarithmic magnitude scale
% ============================================================

figure('Name', 'FFT Comparison - Log Scale', ...
       'Color', 'w');

semilogy(omegaFFT, max(magnitude1, eps), 'LineWidth', 1.2);
hold on;
semilogy(omegaFFT, max(magnitude2, eps), 'LineWidth', 1.2);
hold off;

xlabel('\omega (rad/s)');
ylabel('Vector FFT magnitude (log scale)');
title('Reference vs Amplified E_z FFT');

legend('Reference', 'Amplified', 'Location', 'best');
grid on;
xlim([omegaMin omegaMax]);
xline(centerOmega, '--');

%% ============================================================
% Calculate spectral power
% ============================================================

% Use only the Z component for spectral power and gain
P1 = abs(Fz1).^2;
P2 = abs(Fz2).^2;

%% ============================================================
% Suppress frequencies where reference signal is too weak
% ============================================================

% Fraction of the maximum reference power required to consider
% a frequency reliable.
relativeThreshold = 1e-4;

threshold = max(P1) * relativeThreshold;

validSpectrum = P1 > threshold;

gain = nan(size(P1));
gain(validSpectrum) = P2(validSpectrum) ./ P1(validSpectrum);

gain_dB = 10 * log10(gain);

%% ============================================================
% Select angular-frequency range
% ============================================================

mask = omega >= omegaMin & omega <= omegaMax;

omegaPlot = omega(mask);
gainPlot = gain(mask);
gain_dB_plot = gain_dB(mask);

%% ============================================================
% Plot linear gain
% ============================================================

figure;

plot(omegaPlot, gainPlot, 'LineWidth', 1.5);

xlabel('\omega (rad/s)');
ylabel('Power Gain');

title('E-field Spectral Gain');

grid on;

xlim([omegaMin omegaMax]);
xline(centerOmega, '--');

%% ============================================================
% Plot gain in dB
% ============================================================

figure;

plot(omegaPlot, gain_dB_plot, 'LineWidth', 1.5);

xlabel('\omega (rad/s)');
ylabel('Gain (dB)');

title('E-field Spectral Gain');

grid on;

xlim([omegaMin omegaMax]);
xline(centerOmega, '--');

%% ============================================================
% Print useful information
% ============================================================

fprintf('\n');
fprintf('Sampling frequency : %.6e Hz\n', Fs);
fprintf('Sampling angular frequency : %.6e rad/s\n', 2*pi*Fs);
fprintf('Center frequency   : %.6e Hz\n', centerFrequency);
fprintf('Center angular frequency : %.6e rad/s\n', centerOmega);
fprintf('Angular frequency range : %.6e - %.6e rad/s\n', ...
    omegaMin, omegaMax);
fprintf('Angular frequency width : %.6e rad/s\n', omegaWidth);

%% ============================================================
% Report maximum gain
% ============================================================

[maxGain_dB, maxIndex] = max(gain_dB_plot);

fprintf('\nMaximum gain:\n');
fprintf('  Omega      = %.6e rad/s\n', omegaPlot(maxIndex));
fprintf('  Frequency  = %.6e Hz\n', omegaPlot(maxIndex) / (2*pi));
fprintf('  Gain       = %.3f dB\n', maxGain_dB);
fprintf('  Linear     = %.6e\n', gainPlot(maxIndex));

%% ============================================================
% Complex propagation factor
% ============================================================

% Separation between probes in FDTD length units
L = 200;

% Complex field propagation ratio using the Z component:
%
%     E2 / E1 = exp(-gamma * L)
%
% with
%
%     gamma = alpha + i*beta
%
% Hence:
%
%     alpha = -log(|E2/E1|) / L
%     beta  = -angle(E2/E1) / L

propagationRatio = Fz2 ./ Fz1;

% Only use frequencies where the reference field is sufficiently strong.
validGamma = validSpectrum & ...
             isfinite(propagationRatio) & ...
             (abs(propagationRatio) > 0);

alpha = nan(size(propagationRatio));
beta  = nan(size(propagationRatio));

alpha(validGamma) = -log(abs(propagationRatio(validGamma))) / L;
beta(validGamma)  = -angle(propagationRatio(validGamma)) / L;

% Complex propagation constant
gamma = alpha + 1i * beta;

% Propagation factor over the probe separation
propagationFactor = exp(-gamma * L);

%% ============================================================
% Select angular-frequency range
% ============================================================

maskGamma = mask & validGamma;

omegaGamma = omega(maskGamma);

gammaMagnitude = abs(propagationFactor(maskGamma));
gammaPhase = angle(propagationFactor(maskGamma));
alphaPlot = alpha(maskGamma);
betaPlot = beta(maskGamma);

%% ============================================================
% Plot propagation-factor magnitude
% ============================================================

figure;

plot(omegaGamma, gammaMagnitude, 'LineWidth', 1.5);

xlabel('\omega (rad/s)');
ylabel('|exp(-\gamma L)|');
title('Angular-Frequency-Dependent Propagation Factor Magnitude');

grid on;
xlim([omegaMin omegaMax]);
xline(centerOmega, '--');

%% ============================================================
% Plot propagation-factor phase
% ============================================================

figure;

plot(omegaGamma, gammaPhase, 'LineWidth', 1.5);

xlabel('\omega (rad/s)');
ylabel('Phase (rad)');
title('Angular-Frequency-Dependent Propagation Factor Phase');

grid on;
xlim([omegaMin omegaMax]);
xline(centerOmega, '--');

%% ============================================================
% Plot alpha
% ============================================================

figure;

plot(omegaGamma, alphaPlot, 'LineWidth', 1.5);

xlabel('\omega (rad/s)');
ylabel('\alpha (1 / length unit)');
title('Angular-Frequency-Dependent Attenuation / Gain Coefficient');

grid on;
xlim([omegaMin omegaMax]);
xline(centerOmega, '--');

%% ============================================================
% Plot beta
% ============================================================

figure;

plot(omegaGamma, betaPlot, 'LineWidth', 1.5);

xlabel('\omega (rad/s)');
ylabel('\beta (rad / length unit)');
title('Angular-Frequency-Dependent Propagation Coefficient');

grid on;
xlim([omegaMin omegaMax]);
xline(centerOmega, '--');

%% ============================================================
% Report center-frequency propagation parameters
% ============================================================

centerIndices = find(validGamma);

if ~isempty(centerIndices)
    [~, nearest] = min(abs(omega(centerIndices) - centerOmega));
    centerIndex = centerIndices(nearest);

    fprintf('\nPropagation parameters at center frequency:\n');
    fprintf('  Omega      = %.6e rad/s\n', centerOmega);
    fprintf('  Frequency  = %.6e Hz\n', centerOmega / (2*pi));
    fprintf('  alpha      = %.6e 1 / length unit\n', alpha(centerIndex));
    fprintf('  beta       = %.6e rad / length unit\n', beta(centerIndex));
    fprintf('  |factor|   = %.6e\n', abs(propagationFactor(centerIndex)));
    fprintf('  phase      = %.6e rad\n', angle(propagationFactor(centerIndex)));
end