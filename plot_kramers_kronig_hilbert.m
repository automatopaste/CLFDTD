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
confidencePower = 1;


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
% ============================================================

H = F2 ./ F1;

expMinusAlphaLRaw = abs(H);


%% ============================================================
% Frequency selection
% ============================================================

mask = omega >= omegaMin & omega <= omegaMax;


%% ============================================================
% Phase
%
% IMPORTANT:
% Only unwrap phase where the spectral data is reliable.
% Do not unwrap the entire FFT.
% ============================================================

phaseMeasured = nan(size(omega));

phaseValid = mask & confidence >= 0.5;

phaseMeasured(phaseValid) = ...
    unwrap(angle(H(phaseValid)));


%% ============================================================
% Remove the background propagation phase
%
% The reference phase is:
%
%   Delta(beta*l) = beta*l - n*omega*l/c0
%
% Here beta*l is taken with the opposite sign convention from the
% previous version:
%
%   beta*l = +angle(H)
%
% Only the contiguous region with reliable phase data is retained.
% ============================================================

% Centre-frequency index.
[~, centerIndex] = min(abs(omega - omega0));

% Identify the valid phase interval.
validPhaseIndices = find(phaseValid);

if isempty(validPhaseIndices)
    error('No valid phase data was found.');
end

validPhaseMin = validPhaseIndices(1);
validPhaseMax = validPhaseIndices(end);

validPhaseRange = false(size(omega));
validPhaseRange(validPhaseMin:validPhaseMax) = ...
    phaseValid(validPhaseMin:validPhaseMax);

% Background propagation phase.
betaLBackground = n .* omega .* l ./ c0;

% Gain-induced dispersive phase.
betaLRaw = phaseMeasured - betaLBackground;

% Reference the residual phase to zero at omega0 when omega0 is valid.
if validPhaseRange(centerIndex)
    betaLRaw = betaLRaw - betaLRaw(centerIndex);
end

% Never extrapolate the measured phase outside the valid interval.
betaLRaw(~validPhaseRange) = NaN;

fprintf('Valid phase range: %.6e to %.6e rad/s\n', ...
    omega(validPhaseMin), omega(validPhaseMax));

%% ============================================================
% Kramers-Kronig / Hilbert-transform dispersive phase
%
% H(w) = exp(-alpha(w)*L) * exp(-i*beta(w)*L)
%
% Therefore:
%
%   Re{log(H)} = log(|H|) = -alpha(w)*L
%
% and the corresponding dispersive phase is obtained from
% the Hilbert transform of log(|H|).
% ============================================================

% ------------------------------------------------------------
% Measured logarithmic amplitude
%
% IMPORTANT:
% Use the RAW propagation amplitude here.
% Do not use the confidence-normalised expMinusAlphaL.
% ------------------------------------------------------------

logAmplitude = log(max(expMinusAlphaLRaw, realmin));

% Make sure it is a column vector.
logAmplitude = logAmplitude(:);

%% ============================================================
% Construct two-sided even log-amplitude spectrum
%
% log|H(-w)| = log|H(w)|
%
% The positive-frequency spectrum contains:
%
%   DC, positive frequencies, Nyquist
%
% We mirror everything except DC and Nyquist.
% ============================================================

if mod(N1, 2) == 0

    % N1 even
    %
    % numFreq = N1/2 + 1
    %
    % Example:
    %
    % [DC, positive..., Nyquist]
    %
    % becomes
    %
    % [DC, positive..., Nyquist, negative...]
    
    logAmplitudeFull = [ ...
        logAmplitude; ...
        logAmplitude(end-1:-1:2) ...
    ];

else

    % N1 odd
    %
    % No Nyquist bin exists.
    
    logAmplitudeFull = [ ...
        logAmplitude; ...
        logAmplitude(end:-1:2) ...
    ];

end

%% ============================================================
% Hilbert transform
% ============================================================

analyticLogAmplitude = hilbert(logAmplitudeFull);

% MATLAB:
%
%   imag(hilbert(x))
%
% is the Hilbert transform of x.
%
% With:
%
%   H = exp(-alpha L) exp(-i beta L)
%
% use the corresponding negative sign.

kkPhaseFull = -imag(analyticLogAmplitude);

% Keep positive-frequency portion.
kkPhaseRaw = kkPhaseFull(1:numFreq);

% Force column orientation.
kkPhaseRaw = kkPhaseRaw(:);

%% ============================================================
% Remove arbitrary KK phase offset
% ============================================================

kkOffsetMask = ...
    mask & ...
    ( ...
        (omega >= omegaMin & ...
         omega <= omegaMin + 0.20*omegaWidth) | ...
        (omega >= omegaMax - 0.20*omegaWidth & ...
         omega <= omegaMax) ...
    );

if any(kkOffsetMask)

    kkOffset = median( ...
        kkPhaseRaw(kkOffsetMask) ...
    );

else

    kkOffset = 0;

end

kkPhaseRaw = kkPhaseRaw - kkOffset;

%% ============================================================
% Remove remaining constant phase reference
% ============================================================

[~, centerIndex] = min(abs(omega - omega0));

kkPhaseRaw = ...
    kkPhaseRaw - kkPhaseRaw(centerIndex);

%% ============================================================
% Apply spectral confidence
%
% This is ONLY for displaying the result.
% The KK transform itself used the raw measured amplitude.
% ============================================================

kkPhase = confidence .* kkPhaseRaw;

%% ============================================================
% Confidence-normalised propagation quantities
%
% These are used only for plotting.
%
% The Kramers-Kronig transform above MUST use the raw
% expMinusAlphaLRaw, not this confidence-normalised version.
% ============================================================

% Amplitude -> 1 where the spectrum is unreliable
expMinusAlphaL = ...
    confidence .* expMinusAlphaLRaw + ...
    (1 - confidence);

% Phase -> 0 where the spectrum is unreliable
betaL = ...
    confidence .* betaLRaw;

%% ============================================================
% Select desired frequency range
% ============================================================

mask = omega >= omegaMin & omega <= omegaMax;

omegaPlot = omega(mask);

expMinusAlphaLPlot = expMinusAlphaL(mask);
betaLPlot = betaL(mask);
kkPhasePlot = kkPhase(mask);

confidencePlot = confidence(mask);

%% ============================================================
% Plot Kramers-Kronig dispersive phase
% ============================================================

figure;

plot( ...
    omegaPlot / 1e15, ...
    kkPhasePlot * (180 / pi), ...
    'LineWidth', 1.5);

hold on;

yline(0, '--');
xline(omega0 / 1e15, '--');

xlabel('\omega (10^{15} rad/s)');
ylabel('\Delta(\beta L) [deg]');
title('Kramers-Kronig / Hilbert Dispersive Phase');

grid on;

xlim([omegaMin omegaMax] / 1e15);

%% ============================================================
% Compare measured and KK dispersive phase
% ============================================================

figure;

plot( ...
    omegaPlot / 1e15, ...
    betaLPlot * (180 / pi), ...
    'LineWidth', 1.2);

hold on;

plot( ...
    omegaPlot / 1e15, ...
    kkPhasePlot * (180 / pi), ...
    'LineWidth', 1.8);

yline(0, '--');
xline(omega0 / 1e15, '--');

xlabel('\omega (10^{15} rad/s)');
ylabel('Dispersive phase [deg]');

legend( ...
    'Measured phase', ...
    'Kramers-Kronig phase', ...
    'Location', 'best' ...
);

title('Measured vs Kramers-Kronig Dispersive Phase');

grid on;

xlim([omegaMin omegaMax] / 1e15);