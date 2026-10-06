clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

c0 = 299792458;          % m/s

% FDTD sampling
dx0 = 6.2e-10;           % m
courant = 0.5;
dt = courant * dx0 / c0;

% Medium
n = 3.59;

% Reference wavelength
lambda0 = 890e-9;        % m

% Probe separation
l = lambda0 / n;

% E-field component
% 1 = Ex
% 2 = Ey
% 3 = Ez
component = 3;

% FFT zero padding
fftMultiplier = 1;

% Ignore frequencies where reference spectrum is negligible
thresholdFraction = 1e-1;

%% ============================================================
% Select files
%
% Gain simulation:
%   gain_x1
%   gain_x2
%
% Reference/passive simulation:
%   ref_x1
%   ref_x2
% ============================================================

[fileGain1, pathGain1] = uigetfile( ...
    '*.*', ...
    'Select GAIN E-field recording at x1');

if isequal(fileGain1, 0)
    return;
end

[fileGain2, pathGain2] = uigetfile( ...
    '*.*', ...
    'Select GAIN E-field recording at x2');

if isequal(fileGain2, 0)
    return;
end

[fileRef1, pathRef1] = uigetfile( ...
    '*.*', ...
    'Select REFERENCE/PASSIVE E-field recording at x1');

if isequal(fileRef1, 0)
    return;
end

[fileRef2, pathRef2] = uigetfile( ...
    '*.*', ...
    'Select REFERENCE/PASSIVE E-field recording at x2');

if isequal(fileRef2, 0)
    return;
end

%% ============================================================
% Load files
% ============================================================

fid = fopen(fullfile(pathGain1, fileGain1), 'rb');

if fid == -1
    error('Could not open gain x1 file.');
end

dataGain1 = fread(fid, inf, 'float32=>double');
fclose(fid);


fid = fopen(fullfile(pathGain2, fileGain2), 'rb');

if fid == -1
    error('Could not open gain x2 file.');
end

dataGain2 = fread(fid, inf, 'float32=>double');
fclose(fid);


fid = fopen(fullfile(pathRef1, fileRef1), 'rb');

if fid == -1
    error('Could not open reference x1 file.');
end

dataRef1 = fread(fid, inf, 'float32=>double');
fclose(fid);


fid = fopen(fullfile(pathRef2, fileRef2), 'rb');

if fid == -1
    error('Could not open reference x2 file.');
end

dataRef2 = fread(fid, inf, 'float32=>double');
fclose(fid);

%% ============================================================
% Number of Vec3 samples
% ============================================================

NGain1 = floor(numel(dataGain1) / 3);
NGain2 = floor(numel(dataGain2) / 3);

NRef1 = floor(numel(dataRef1) / 3);
NRef2 = floor(numel(dataRef2) / 3);

N = min([NGain1, NGain2, NRef1, NRef2]);

fprintf('Gain x1 samples = %d\n', NGain1);
fprintf('Gain x2 samples = %d\n', NGain2);
fprintf('Ref  x1 samples = %d\n', NRef1);
fprintf('Ref  x2 samples = %d\n', NRef2);
fprintf('Using            = %d\n', N);

%% ============================================================
% Extract selected component
% ============================================================

idx = component:3:(3*N);

EGain1 = dataGain1(idx);
EGain2 = dataGain2(idx);

ERef1 = dataRef1(idx);
ERef2 = dataRef2(idx);

EGain1 = EGain1(:);
EGain2 = EGain2(:);

ERef1 = ERef1(:);
ERef2 = ERef2(:);

clear dataGain1 dataGain2 dataRef1 dataRef2;

%% ============================================================
% Remove DC
% ============================================================

EGain1 = EGain1 - mean(EGain1);
EGain2 = EGain2 - mean(EGain2);

ERef1 = ERef1 - mean(ERef1);
ERef2 = ERef2 - mean(ERef2);

%% ============================================================
% Apply identical window
% ============================================================

% Currently disabled, matching your original script.
%
% If enabled, the SAME window must be applied to all four signals.

window = hann(N);

%EGain1 = EGain1 .* window;
%EGain2 = EGain2 .* window;
%ERef1  = ERef1  .* window;
%ERef2  = ERef2  .* window;

%% ============================================================
% FFT
% ============================================================

fftPoints = 2^nextpow2(fftMultiplier * N);

EGain1_fft = fft(EGain1, fftPoints);
EGain2_fft = fft(EGain2, fftPoints);

ERef1_fft = fft(ERef1, fftPoints);
ERef2_fft = fft(ERef2, fftPoints);

%% ============================================================
% Frequency axis
% ============================================================

freq = (0:fftPoints-1).' / (fftPoints * dt);

omega = 2*pi*freq;

%% Positive frequencies

positive = ...
    freq > 0 & ...
    freq < 1/(2*dt);

freq = freq(positive);
omega = omega(positive);

EGain1_fft = EGain1_fft(positive);
EGain2_fft = EGain2_fft(positive);

ERef1_fft = ERef1_fft(positive);
ERef2_fft = ERef2_fft(positive);

%% ============================================================
% Find spectral peak
% ============================================================

[~, peakIndex] = max(abs(EGain1_fft));

fprintf('\nSpectral peak:\n');
fprintf('Frequency  = %.6f THz\n', ...
    freq(peakIndex) * 1e-12);

fprintf('Omega      = %.6e rad/s\n', ...
    omega(peakIndex));

fprintf('Expected   = %.6e rad/s\n', ...
    2*pi*c0/lambda0);

%% ============================================================
% Calculate propagation transfer functions
%
% Gain:
%
% H_gain = E_gain(x2) / E_gain(x1)
%
% Reference:
%
% H_ref  = E_ref(x2) / E_ref(x1)
% ============================================================

gainMagnitude = abs(EGain1_fft);
refMagnitude  = abs(ERef1_fft);

thresholdGain = thresholdFraction * max(gainMagnitude);
thresholdRef  = thresholdFraction * max(refMagnitude);

valid = ...
    gainMagnitude > thresholdGain & ...
    refMagnitude  > thresholdRef;

H_gain = nan(size(EGain1_fft));
H_ref  = nan(size(ERef1_fft));

H_gain(valid) = ...
    EGain2_fft(valid) ./ EGain1_fft(valid);

H_ref(valid) = ...
    ERef2_fft(valid) ./ ERef1_fft(valid);

%% ============================================================
% Normalised gain
%
% G_norm =
%
% |H_gain / H_ref|
%
% This removes the passive propagation response.
% ============================================================

normalisedGain = nan(size(H_gain));

normalisedGain(valid) = ...
    abs(H_gain(valid) ./ H_ref(valid));

%% ============================================================
% Gain in dB
% ============================================================

gainDB = nan(size(normalisedGain));

gainDB(valid) = ...
    20 * log10(normalisedGain(valid));

%% ============================================================
% Propagation constants
%
% H = exp(-gamma*l)
%
% gamma = alpha + j*beta
% ============================================================

gammaGain = nan(size(H_gain));
gammaRef  = nan(size(H_ref));

gammaGain(valid) = ...
    -log(H_gain(valid)) / l;

gammaRef(valid) = ...
    -log(H_ref(valid)) / l;

alphaGain = real(gammaGain);
betaGain  = imag(gammaGain);

alphaRef = real(gammaRef);
betaRef  = imag(gammaRef);

%% ============================================================
% Unwrap propagation phase
%
% beta*l = -unwrap(angle(H))
% ============================================================

betaLGain = nan(size(betaGain));
betaLRef  = nan(size(betaRef));

validIndices = find(valid);

if ~isempty(validIndices)

    breaks = [ ...
        1; ...
        find(diff(validIndices) > 1) + 1 ...
    ];

    ends = [ ...
        breaks(2:end)-1; ...
        numel(validIndices) ...
    ];

    for k = 1:numel(breaks)

        ii = validIndices(breaks(k):ends(k));

        betaLGain(ii) = ...
            -unwrap(angle(H_gain(ii)));

        betaLRef(ii) = ...
            -unwrap(angle(H_ref(ii)));

    end
end

%% ============================================================
% Normalised propagation phase
%
% Difference between gain and reference propagation phase.
% ============================================================

normalisedPhase = nan(size(betaLGain));

normalisedPhase(valid) = ...
    betaLGain(valid) - betaLRef(valid);

%% ============================================================
% Background propagation phase
% ============================================================

backgroundPhase = ...
    n .* omega .* l / c0;

%% ============================================================
% Print parameters
% ============================================================

fprintf('\n============================================\n');
fprintf('Normalised propagation analysis\n');
fprintf('============================================\n');

fprintf('lambda0      = %.6f nm\n', ...
    lambda0*1e9);

fprintf('n            = %.6f\n', n);

fprintf('l            = %.6e m\n', l);

fprintf('l            = %.6f nm\n', l*1e9);

fprintf('l/lambda0    = %.6f\n', l/lambda0);

fprintf('dt           = %.6e s\n', dt);

fprintf('FFT points   = %d\n', fftPoints);

fprintf('True FFT resolution = %.6f THz\n', ...
    1/(N*dt)*1e-12);

fprintf('FFT bin spacing     = %.6f THz\n', ...
    1/(fftPoints*dt)*1e-12);

%% ============================================================
% Plot normalised gain
% ============================================================

figure;

plot( ...
    omega(valid), ...
    normalisedGain(valid), ...
    'LineWidth', 1.5);

grid on;

xlabel('\omega (rad/s)');
ylabel('|H_{gain}/H_{ref}|');

title('Normalised Gain');

yline(1.0, ':');

%% ============================================================
% Plot gain in dB
% ============================================================

figure;

plot( ...
    omega(valid), ...
    gainDB(valid), ...
    'LineWidth', 1.5);

grid on;

xlabel('\omega (rad/s)');
ylabel('Gain (dB)');

title('Normalised Gain');

yline(0.0, ':');

%% ============================================================
% Plot normalised propagation phase
% ============================================================

figure;

plot( ...
    omega(valid), ...
    normalisedPhase(valid), ...
    'LineWidth', 1.5);

grid on;

xlabel('\omega (rad/s)');
ylabel('\Delta(\beta l) (rad)');

title('Normalised Propagation Phase');

yline(0.0, ':');

%% ============================================================
% Plot gain and phase together
% ============================================================

figure;

yyaxis left;

plot( ...
    omega(valid), ...
    normalisedGain(valid), ...
    'LineWidth', 1.5);

ylabel('|H_{gain}/H_{ref}|');

yyaxis right;

plot( ...
    omega(valid), ...
    normalisedPhase(valid), ...
    'LineWidth', 1.5);

ylabel('\Delta(\beta l) (rad)');

grid on;

xlabel('\omega (rad/s)');

title('Normalised Propagation Factor');