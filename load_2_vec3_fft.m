clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

% Speed of light
c0 = 299792458;             % m/s

% FDTD temporal sampling interval
dx0 = 6.2e-10 * 4.0;
courant = 0.5;
dt = courant * dx0 / c0;       % seconds

% Frequency to center the plot around
centerFrequency = 3.37e14;  % Hz

% Frequency range displayed around center
frequencyWidth = 2.0e14;    % Hz

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

% Zero-padding factor
fftPadFactor = 8;

Nfft = 2^nextpow2(N * fftPadFactor);

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

frequency = (0:nHalf-1).' * Fs / Nfft;

%% ============================================================
% Plot individual FFTs for visual comparison
% ============================================================

fMin = centerFrequency - frequencyWidth / 2;
fMax = centerFrequency + frequencyWidth / 2;

maskFFT = frequency >= fMin & frequency <= fMax;
fFFT = frequency(maskFFT);

% Individual component magnitudes
components1 = {
    abs(Fx1(maskFFT)), ...
    abs(Fy1(maskFFT)), ...
    abs(Fz1(maskFFT))
};

components2 = {
    abs(Fx2(maskFFT)), ...
    abs(Fy2(maskFFT)), ...
    abs(Fz2(maskFFT))
};

componentNames = {'E_x', 'E_y', 'E_z'};

% Combined vector magnitude spectrum
magnitude1 = sqrt( ...
    abs(Fx1(maskFFT)).^2 + ...
    abs(Fy1(maskFFT)).^2 + ...
    abs(Fz1(maskFFT)).^2);

magnitude2 = sqrt( ...
    abs(Fx2(maskFFT)).^2 + ...
    abs(Fy2(maskFFT)).^2 + ...
    abs(Fz2(maskFFT)).^2);

% Plot each Cartesian component separately
figure('Name', 'Individual FFT Comparison', ...
       'Color', 'w');

tiledlayout(2, 2, 'TileSpacing', 'compact');

for k = 1:3
    nexttile;

    plot(fFFT, components1{k}, 'LineWidth', 1.2);
    hold on;
    plot(fFFT, components2{k}, 'LineWidth', 1.2);
    hold off;

    xlabel('Frequency (Hz)');
    ylabel('|FFT|');
    title(componentNames{k});

    legend('X1', 'X2', 'Location', 'best');
    grid on;
    xlim([fMin fMax]);
    xline(centerFrequency, '--');
end

% Combined vector magnitude
nexttile;

plot(fFFT, magnitude1, 'LineWidth', 1.2);
hold on;
plot(fFFT, magnitude2, 'LineWidth', 1.2);
hold off;

xlabel('Frequency (Hz)');
ylabel('Vector FFT magnitude');
title('Combined E-field');

legend('X2', 'X2', 'Location', 'best');
grid on;
xlim([fMin fMax]);
xline(centerFrequency, '--');

%% ============================================================
% Plot individual FFTs on logarithmic magnitude scale
% ============================================================

figure('Name', 'FFT Comparison - Log Scale', ...
       'Color', 'w');

semilogy(fFFT, max(magnitude1, eps), 'LineWidth', 1.2);
hold on;
semilogy(fFFT, max(magnitude2, eps), 'LineWidth', 1.2);
hold off;

xlabel('Frequency (Hz)');
ylabel('Vector FFT magnitude (log scale)');
title('X1 vs X2 E-field FFT');

legend('X1', 'X2', 'Location', 'best');
grid on;
xlim([fMin fMax]);
xline(centerFrequency, '--');

%% ============================================================
% Calculate spectral power
% ============================================================

P1 = abs(Fx1).^2 + ...
     abs(Fy1).^2 + ...
     abs(Fz1).^2;

P2 = abs(Fx2).^2 + ...
     abs(Fy2).^2 + ...
     abs(Fz2).^2;

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
% Select frequency range
% ============================================================

fMin = centerFrequency - frequencyWidth / 2;
fMax = centerFrequency + frequencyWidth / 2;

mask = frequency >= fMin & frequency <= fMax;

fPlot = frequency(mask);
gainPlot = gain(mask);
gain_dB_plot = gain_dB(mask);

%% ============================================================
% Convert frequency to wavelength
% ============================================================

wavelength = c0 ./ fPlot;

% Convert to nm
wavelength_nm = wavelength * 1e9;

%% ============================================================
% Sort wavelength from low -> high
% ============================================================

[wavelength_nm, sortIndex] = sort(wavelength_nm);

gainPlot = gainPlot(sortIndex);
gain_dB_plot = gain_dB_plot(sortIndex);

%% ============================================================
% Plot linear gain
% ============================================================

figure;

plot(wavelength_nm, gainPlot, 'LineWidth', 1.5);

xlabel('Wavelength (nm)');
ylabel('Power Gain');

title('E-field Spectral Gain');

grid on;

xline(c0 / centerFrequency * 1e9, '--');

%% ============================================================
% Plot gain in dB
% ============================================================

figure;

plot(wavelength_nm, gain_dB_plot, 'LineWidth', 1.5);

xlabel('Wavelength (nm)');
ylabel('Gain (dB)');

title('E-field Spectral Gain');

grid on;

xline(c0 / centerFrequency * 1e9, '--');

%% ============================================================
% Print useful information
% ============================================================

centerWavelength_nm = c0 / centerFrequency * 1e9;

fprintf('\n');
fprintf('Sampling frequency : %.6e Hz\n', Fs);
fprintf('Center frequency   : %.6e Hz\n', centerFrequency);
fprintf('Center wavelength  : %.3f nm\n', centerWavelength_nm);
fprintf('Frequency range    : %.6e - %.6e Hz\n', fMin, fMax);
fprintf('Wavelength range   : %.3f - %.3f nm\n', ...
    min(wavelength_nm), max(wavelength_nm));

%% ============================================================
% Report maximum gain
% ============================================================

[maxGain_dB, maxIndex] = max(gain_dB_plot);

fprintf('\nMaximum gain:\n');
fprintf('  Wavelength = %.3f nm\n', wavelength_nm(maxIndex));
fprintf('  Gain       = %.3f dB\n', maxGain_dB);
fprintf('  Linear     = %.6e\n', gainPlot(maxIndex));

%% ============================================================
% Gain coefficient
% ============================================================

% Separation between probes in FDTD length units
L = 200;

% Power gain coefficient [1 / length unit]
gainCoefficient = log(gainPlot) / L;

% Gain coefficient in dB / length unit
gain_dB_per_length = gain_dB_plot / L;

%% ============================================================
% Plot gain coefficient
% ============================================================

figure;

plot(wavelength_nm, gainCoefficient, 'LineWidth', 1.5);

xlabel('Wavelength (nm)');
ylabel('Gain coefficient (1 / length unit)');

title('Wavelength-dependent Gain Coefficient');

grid on;

xline(c0 / centerFrequency * 1e9, '--');

%% ============================================================
% dB gain coefficient
% ============================================================

figure;

plot(wavelength_nm, gain_dB_per_length, 'LineWidth', 1.5);

xlabel('Wavelength (nm)');
ylabel('Gain coefficient (dB / length unit)');

title('Wavelength-dependent Gain Coefficient');

grid on;

xline(c0 / centerFrequency * 1e9, '--');