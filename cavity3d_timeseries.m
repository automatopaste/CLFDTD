clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

c0 = 299792458;               % Speed of light (m/s)

% FDTD parameters
dx0 = 1.363509749e-6 / 400.0; % Cell spacing (m)
courant = 0.5;
dt = courant * dx0 / c0;      % Timestep (s)

% Select monitor component to plot:
% 1 = Ex, 2 = Ey, 3 = Ez
% Set to 0 to plot all three components.
component = 0;

% FFT options
removeDC = true;              % Remove mean before FFT
useHannWindow = false;         % Reduce spectral leakage
normalizeFFT = true;         % Normalize peak magnitude to 1

% Cavity resonance markers
cavityLength = 12.4e-6;  % Physical cavity length (m)
refractiveIndex = 3.59;  % Cavity refractive index
showResonances = false;

% Resonance frequency: f_m = m*c0/(2*n*L)
% The mode number m is calculated automatically for the FFT range.

%% ============================================================
% Select and load binary vec3 monitor
% ============================================================

[file, path] = uigetfile( ...
{'*.bin;*.dat;*.*', 'Binary monitor files'}, ...
'Select point monitor file');

if isequal(file, 0)
disp('No file selected.');
return;
end

filename = fullfile(path, file);

fid = fopen(filename, 'rb');

if fid == -1
error('Could not open file: %s', filename);
end

% Each sample consists of three float32 values: Ex, Ey, Ez.
data = fread(fid, [3, inf], '3*single=>double');
fclose(fid);

if isempty(data)
error('The selected file contains no samples.');
end

if mod(numel(data), 3) ~= 0
error('File size is inconsistent with vec3 samples.');
end

%% ============================================================
% Extract components and construct time axis
% ============================================================

Ex = data(1, :).';
Ey = data(2, :).';
Ez = data(3, :).';

numSamples = size(data, 2);

time = (0:numSamples-1).' * dt;

fprintf('Loaded: %s\n', filename);
fprintf('Samples: %d\n', numSamples);
fprintf('Timestep: %.6e s\n', dt);
fprintf('Total duration: %.6e s (%.6f ps)\n', ...
numSamples * dt, numSamples * dt * 1e12);
fprintf('FFT bin spacing: %.6e THz\n', ...
(1 / (numSamples * dt)) / 1e12);

%% ============================================================
% Select components
% ============================================================

switch component
case 0
signals = {Ex, Ey, Ez};
labels = {'E_x', 'E_y', 'E_z'};

case 1
    signals = {Ex};
    labels = {'E_x'};

case 2
    signals = {Ey};
    labels = {'E_y'};

case 3
    signals = {Ez};
    labels = {'E_z'};

otherwise
    error('component must be 0, 1, 2, or 3.');

end

%% ============================================================
% Plot time-domain signals and FFT spectra
% ============================================================

for c = 1:numel(signals)

signal = signals{c};

%% Time-domain plot
figure('Color', 'w', 'Name', ...
    [labels{c}, ' Time Domain']);

plot(time * 1e12, signal, 'LineWidth', 1);
grid on;
xlabel('Time (ps)');
ylabel('Field amplitude');
title(['Point Monitor: ', labels{c}], ...
    'Interpreter', 'tex');

%% Prepare signal for FFT
fftSignal = signal;

if removeDC
    fftSignal = fftSignal - mean(fftSignal);
end

if useHannWindow
    window = hann(numSamples);
    fftSignal = fftSignal .* window;
else
    window = ones(numSamples, 1);
end

%% Compute single-sided amplitude spectrum
Y = fft(fftSignal);

nPositive = floor(numSamples / 2) + 1;

% Correct for window coherent gain to retain amplitude scaling.
amplitude = abs(Y(1:nPositive)) / sum(window);

if rem(numSamples, 2) == 0
    % Even length: do not double DC or Nyquist.
    if nPositive > 2
        amplitude(2:end-1) = 2 * amplitude(2:end-1);
    end
else
    % Odd length: no exact Nyquist bin.
    if nPositive > 1
        amplitude(2:end) = 2 * amplitude(2:end);
    end
end

% Frequency vector in THz
frequency = (0:nPositive-1).' / (numSamples * dt);
frequencyTHz = frequency / 1e12;

if normalizeFFT
    peakAmplitude = max(amplitude);
    if peakAmplitude > 0
        amplitude = amplitude / peakAmplitude;
    end
end

%% Plot FFT
figure('Color', 'w', 'Name', ...
    [labels{c}, ' FFT']);

plot(frequencyTHz, amplitude, 'LineWidth', 1);
hold on;
grid on;

xlabel('Frequency (THz)');

if normalizeFFT
    ylabel('Normalized magnitude');
else
    ylabel('Single-sided amplitude');
end

title([labels{c}, ' FFT Spectrum'], ...
    'Interpreter', 'tex');

maxFrequencyTHz = 0.5 / dt / 1e12;
maxFrequencyTHz = 1000;
xlim([0, maxFrequencyTHz]);

%% Add resonant-mode xlines
if showResonances
    % Free spectral range in Hz
    FSR = c0 / (2 * refractiveIndex * cavityLength);

    % Positive mode numbers within the plotted frequency range.
    maxMode = floor(maxFrequencyTHz * 1e12 / FSR);
    modes = (1:maxMode).';

    resonanceHz = modes * FSR;
    resonanceTHz = resonanceHz / 1e12;

    for k = 1:numel(modes)
        xline(resonanceTHz(k), '--r', ...
            sprintf('m=%d', modes(k)), ...
            'LabelVerticalAlignment', 'middle', ...
            'LabelOrientation', 'horizontal', ...
            'HandleVisibility', 'off');
    end

    fprintf('Resonance free spectral range: %.6f THz\\n', ...
        FSR / 1e12);
    fprintf('Resonant modes plotted: %d\\n', numel(modes));
end

hold off;

end
