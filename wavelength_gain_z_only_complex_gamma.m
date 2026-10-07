clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

c0 = 299792458;          % m/s

% FDTD sampling
dx0 = 6.2e-10;           % m
courant = 0.1;
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

% ============================================================
% FFT configuration
% ============================================================

% Zero-padding multiplier.
%
% This increases the density of the frequency grid, but does
% NOT increase the true physical frequency resolution.
fftMultiplier = 1;

% ============================================================
% Frequency region of interest
% ============================================================

% Desired centre frequency
omega0 = 2.12e15;        % rad/s

% Desired total width
omegaWidth = 0.60e15;    % rad/s

omegaMin = omega0 - omegaWidth/2;
omegaMax = omega0 + omegaWidth/2;

% ============================================================
% Reference spectrum threshold
% ============================================================

% Ignore frequencies where the reference spectrum at x1 is
% too small to produce a reliable transfer-function ratio.
%
% 1e-3 = 0.1% of the reference spectral peak.
%
% The previous value of 1e-1 (10%) was very aggressive and
% could remove most of the desired frequency window.
thresholdFraction = 1e-3;


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

%EGain1 = EGain1 - mean(EGain1);
%EGain2 = EGain2 - mean(EGain2);

%ERef1 = ERef1 - mean(ERef1);
%ERef2 = ERef2 - mean(ERef2);


%% ============================================================
% Apply identical window
% ============================================================

% A window reduces spectral leakage but also changes the
% effective spectral response.
%
% Currently disabled to preserve the original behaviour.
%
% If enabled, the SAME window must be applied to all four
% signals.

window = hann(N);

% EGain1 = EGain1 .* window;
% EGain2 = EGain2 .* window;
% ERef1  = ERef1  .* window;
% ERef2  = ERef2  .* window;


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


%% ============================================================
% Positive frequencies
% ============================================================

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
% Frequency resolution information
% ============================================================

trueDeltaOmega = 2*pi / (N * dt);

fftDeltaOmega = 2*pi / (fftPoints * dt);

fprintf('\n============================================\n');
fprintf('Frequency resolution\n');
fprintf('============================================\n');

fprintf('Samples             = %d\n', N);
fprintf('Total time          = %.6e s\n', N*dt);

fprintf('True resolution     = %.6e rad/s\n', ...
    trueDeltaOmega);

fprintf('Zero-padded spacing = %.6e rad/s\n', ...
    fftDeltaOmega);

fprintf('True resolution     = %.6f THz\n', ...
    trueDeltaOmega / (2*pi) * 1e-12);

fprintf('Grid spacing        = %.6f THz\n', ...
    fftDeltaOmega / (2*pi) * 1e-12);

fprintf('Window              = %.6e to %.6e rad/s\n', ...
    omegaMin, omegaMax);


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

% Reference-based threshold.
%
% Only the reference spectrum is used to determine whether
% the frequency has enough signal for a reliable ratio.

thresholdRef = thresholdFraction * max(refMagnitude);

validReference = ...
    refMagnitude > thresholdRef;


%% ============================================================
% Restrict analysis to desired frequency window
% ============================================================

frequencyWindow = ...
    omega >= omegaMin & ...
    omega <= omegaMax;


%% ============================================================
% Final validity mask
% ============================================================

valid = ...
    validReference & ...
    frequencyWindow;


%% ============================================================
% Calculate transfer functions
% ============================================================

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
% Print analysis information
% ============================================================

numWindowBins = sum(frequencyWindow);
numValidBins = sum(valid);

fprintf('\n============================================\n');
fprintf('Normalised propagation analysis\n');
fprintf('============================================\n');

fprintf('lambda0             = %.6f nm\n', ...
    lambda0*1e9);

fprintf('n                   = %.6f\n', n);

fprintf('l                   = %.6e m\n', l);

fprintf('l                   = %.6f nm\n', l*1e9);

fprintf('l/lambda0           = %.6f\n', l/lambda0);

fprintf('dt                  = %.6e s\n', dt);

fprintf('FFT points          = %d\n', fftPoints);

fprintf('True FFT resolution = %.6e rad/s\n', ...
    trueDeltaOmega);

fprintf('FFT grid spacing    = %.6e rad/s\n', ...
    fftDeltaOmega);

fprintf('\n');

fprintf('Requested centre    = %.6e rad/s\n', ...
    omega0);

fprintf('Requested width     = %.6e rad/s\n', ...
    omegaWidth);

fprintf('Window minimum      = %.6e rad/s\n', ...
    omegaMin);

fprintf('Window maximum      = %.6e rad/s\n', ...
    omegaMax);

fprintf('Window bins         = %d\n', ...
    numWindowBins);

fprintf('Valid bins          = %d\n', ...
    numValidBins);

fprintf('Reference threshold = %.3g %% of peak\n', ...
    thresholdFraction * 100);


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

xlim([omegaMin omegaMax]);


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

xlim([omegaMin omegaMax]);


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

xlim([omegaMin omegaMax]);


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

xlim([omegaMin omegaMax]);
