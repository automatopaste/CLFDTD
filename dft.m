clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

c0 = 299792458;          % m/s

% FDTD sampling
dx0 = 6.2e-10;            % m
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

% Desired centre frequency
omega0 = 2.12e15;        % rad/s

% Desired total frequency width
omegaWidth = 0.60e15;    % rad/s

% Frequency limits
omegaMin = omega0 - omegaWidth/2;
omegaMax = omega0 + omegaWidth/2;


%% ============================================================
% Spectral confidence configuration
% ============================================================

% Below this fraction of the peak FFT magnitude, the result
% progressively loses confidence and is driven toward 1.
%
% Example:
%   1e-2 = confidence starts decreasing below 1% of peak
%   1e-3 = confidence starts decreasing below 0.1% of peak
%
confidenceFloor = 1e-2;

% Controls how sharply confidence falls.
%
% 1 = linear
% 2 = smoother
% 4+ = stays near full confidence longer, then falls rapidly
confidencePower = 2;


%% ============================================================
% Select first file
% ============================================================

[file1, path1] = uigetfile('*.*', 'Select FIRST vec3 file');

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

E1 = double(data1(component, :)).';


%% ============================================================
% Select second file
% ============================================================

[file2, path2] = uigetfile('*.*', 'Select SECOND vec3 file');

if isequal(file2, 0)
    return;
end

fid = fopen(fullfile(path2, file2), 'rb');

if fid == -1
    error('Could not open second file.');
end

data2 = fread(fid, 'single');
fclose(fid);

if mod(numel(data2), 3) ~= 0
    error('Second file does not contain a whole number of vec3 samples.');
end

data2 = reshape(data2, 3, []);

E2 = double(data2(component, :)).';


%% ============================================================
% Basic information
% ============================================================

N1 = numel(E1);
N2 = numel(E2);

fprintf('First file:  %d samples\n', N1);
fprintf('Second file: %d samples\n', N2);
fprintf('dt = %.9e s\n', dt);
fprintf('Sampling frequency = %.9e Hz\n', 1/dt);
fprintf('Nyquist frequency = %.9e Hz\n', 1/(2*dt));
fprintf('Probe separation = %.9e m\n', l);

if N1 ~= N2
    error('The two files must contain the same number of samples.');
end


%% ============================================================
% Remove DC offset
% ============================================================

E1 = E1 - mean(E1);
E2 = E2 - mean(E2);


%% ============================================================
% FFT
% ============================================================

F1 = fft(E1);
F2 = fft(E2);

% Only positive frequencies
numFreq = floor(N1/2) + 1;

F1 = F1(1:numFreq);
F2 = F2(1:numFreq);

f = (0:numFreq-1).' / (N1 * dt);

% Angular frequency
omega = 2*pi*f;


%% ============================================================
% FFT magnitude / spectral confidence
% ============================================================

magF1 = abs(F1);
magF2 = abs(F2);

% Use the geometric mean so that BOTH measurements must contain
% sufficient spectral energy for high confidence.
spectralMagnitude = sqrt(magF1 .* magF2);

% Peak spectral magnitude
maxSpectralMagnitude = max(spectralMagnitude);

% Normalize to 0...1
normalizedMagnitude = ...
    spectralMagnitude / maxSpectralMagnitude;

% ------------------------------------------------------------
% Convert normalized magnitude into confidence.
%
% confidence = 1 above confidenceFloor
% confidence -> 0 as magnitude -> 0
% ------------------------------------------------------------

confidence = zeros(size(normalizedMagnitude));

strong = normalizedMagnitude >= confidenceFloor;

confidence(strong) = 1;

weak = ~strong;

% Normalize weak region so:
%
% magnitude = confidenceFloor -> confidence = 1
% magnitude = 0               -> confidence = 0

confidence(weak) = ...
    (normalizedMagnitude(weak) / confidenceFloor) ...
    .^ confidencePower;

% Clamp against numerical roundoff
confidence = max(0, min(1, confidence));


%% ============================================================
% Complex propagation factor
%
% H = E2 / E1
%
% H = exp(-alpha*L) * exp(-i*beta*L)
%
% Therefore:
%
% exp(-alpha*L) = abs(H)
% beta*L        = -angle(H)
% ============================================================

H = F2 ./ F1;

expMinusAlphaLRaw = abs(H);

betaLRaw = -unwrap(angle(H));


%% ============================================================
% Normalize weak spectral regions toward 1.0
% ============================================================

% Strong spectral regions:
%
%     confidence = 1
%     result = raw result
%
% Weak spectral regions:
%
%     confidence -> 0
%     result -> 1
%
% This prevents low-FFT-magnitude bins from producing large
% artificial gain/phase excursions.

expMinusAlphaL = ...
    confidence .* expMinusAlphaLRaw + ...
    (1 - confidence) .* 1.0;

betaL = ...
    confidence .* betaLRaw + ...
    (1 - confidence) .* 1.0;


%% ============================================================
% Select desired frequency range
% ============================================================

mask = omega >= omegaMin & omega <= omegaMax;

omegaPlot = omega(mask);

expMinusAlphaLPlot = expMinusAlphaL(mask);
betaLPlot = betaL(mask);

confidencePlot = confidence(mask);


%% ============================================================
% Convert angular frequency to wavelength
% ============================================================

lambdaPlot = 2*pi*c0 ./ omegaPlot;
lambdaPlot_nm = lambdaPlot * 1e9;


%% ============================================================
% Plot spectral confidence
% ============================================================

figure;

plot(omegaPlot / 1e15, confidencePlot, ...
    'LineWidth', 1.2);

xlabel('\omega (10^{15} rad/s)');
ylabel('Spectral confidence');
title('FFT Spectral Confidence');

grid on;

xlim([omegaMin omegaMax] / 1e15);
ylim([0 1.05]);


%% ============================================================
% Plot exp(-alpha*L) versus angular frequency
% ============================================================

figure;

plot(omegaPlot / 1e15, expMinusAlphaLPlot, ...
    'LineWidth', 1.2);

xlabel('\omega (10^{15} rad/s)');
ylabel('exp(-\alpha L)');
title('Propagation Amplitude Factor');

grid on;

xlim([omegaMin omegaMax] / 1e15);


%% ============================================================
% Plot exp(-alpha*L) versus wavelength
% ============================================================

figure;

plot(lambdaPlot_nm, expMinusAlphaLPlot, ...
    'LineWidth', 1.2);

xlabel('\lambda (nm)');
ylabel('exp(-\alpha L)');
title('Propagation Amplitude Factor');

grid on;

set(gca, 'XDir', 'reverse');


%% ============================================================
% Plot beta*L versus angular frequency
% ============================================================

figure;

plot(omegaPlot / 1e15, betaLPlot, ...
    'LineWidth', 1.2);

xlabel('\omega (10^{15} rad/s)');
ylabel('\beta L (rad)');
title('Propagation Phase');

grid on;

xlim([omegaMin omegaMax] / 1e15);


%% ============================================================
% Report FFT resolution
% ============================================================

deltaOmega = 2*pi / (N1 * dt);

fprintf('\n');
fprintf('True FFT resolution:\n');
fprintf('  %.6e rad/s\n', deltaOmega);

fprintf('\n');
fprintf('Requested frequency range:\n');
fprintf('  %.6e to %.6e rad/s\n', omegaMin, omegaMax);

fprintf('\n');
fprintf('Number of FFT bins in requested range:\n');
fprintf('  %d\n', sum(mask));


%% ============================================================
% Spectral confidence information
% ============================================================

fprintf('\n');
fprintf('Spectral confidence settings:\n');
fprintf('  Confidence floor: %.3e of peak FFT magnitude\n', ...
    confidenceFloor);

fprintf('  Confidence power: %.2f\n', confidencePower);

fprintf('\n');
fprintf('Maximum spectral magnitude:\n');
fprintf('  %.6e\n', maxSpectralMagnitude);


%% ============================================================
% Reference quantities
% ============================================================

fprintf('\n');
fprintf('Reference wavelength:\n');
fprintf('  %.6f nm\n', lambda0 * 1e9);

fprintf('Wavelength in medium:\n');
fprintf('  %.6f nm\n', lambda0/n * 1e9);

fprintf('Probe separation L:\n');
fprintf('  %.6e m\n', l);