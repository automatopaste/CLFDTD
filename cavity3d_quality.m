clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

% FDTD timestep (use the exact timestep from your simulation)
c0 = 299792458;
dx0 = 1.363509749e-6 / 400.0;
courant = 0.5;
dt = courant * dx0 / c0;

% Monitor file format:
% One float32 sample per timestep, OR interleaved vec3 samples.
% Set fileFormat = 'scalar' for one component per timestep.
% Set fileFormat = 'vec3' for Ex, Ey, Ez per timestep.
fileFormat = 'vec3';

% For vec3 files: 1 = Ex, 2 = Ey, 3 = Ez
component = 3;

% Initial pulse rejection
% Choose the time after which the source pulse is no longer
% driving the cavity. Set to [] to choose interactively.
rejectBefore_ps = 0.12;

% Optional end of fitting interval (ps); [] means use file end.
fitEnd_ps = 0.3;

% Target resonant frequency in Hz.
% Set this to the resonance whose Q factor you want to measure.
f0_Hz = 383e12;  % Example: 383.327 THz

% Full bandwidth of the bandpass filter in Hz.
% Start wider than the expected resonance linewidth.
% Adjust this to exclude neighboring modes without distorting
% the target mode's decay.
targetBandwidth_Hz = 10e12;  % Example: 12 THz

% Fit only samples where the envelope exceeds this fraction of
% its maximum in the retained interval. Set to 0 to disable.
minimumEnvelopeFraction = 0;

%% ============================================================
% Load time-series file
% ============================================================

[file, path] = uigetfile( ...
{'*.bin;*.dat;*.txt;*.csv;*.*', 'Time-series files'}, ...
'Select point-monitor time-series file');

if isequal(file, 0)
disp('No file selected.');
return;
end

filename = fullfile(path, file);

[~,~,ext] = fileparts(filename);

if any(strcmpi(ext, {'.txt', '.csv'}))
raw = readmatrix(filename);

% Text files:
% One column = scalar field samples
% Three columns = Ex, Ey, Ez
if size(raw, 2) == 1
    signal = raw(:,1);
elseif size(raw, 2) >= 3
    signal = raw(:,component);
else
    error('Text file must contain one or three columns.');
end

signal = signal(:);

else
fid = fopen(filename, 'rb');

if fid == -1
    error('Could not open file: %s', filename);
end

raw = fread(fid, inf, 'single=>double');
fclose(fid);

if strcmpi(fileFormat, 'scalar')
    signal = raw;

elseif strcmpi(fileFormat, 'vec3')
    if mod(numel(raw), 3) ~= 0
        error('File size is not divisible by 3 float32 values.');
    end

    data = reshape(raw, 3, []);
    signal = data(component, :).';

else
    error('fileFormat must be scalar or vec3.');
end

end

signal = double(signal(:));

if numel(signal) < 20
error('Not enough samples for envelope analysis.');
end

% Time axis
N = numel(signal);
time = (0:N-1).' * dt;
time_ps = time * 1e12;

fprintf('Loaded %d samples from %s\n', N, filename);
fprintf('Timestep: %.6g ps\n', dt * 1e12);
fprintf('Total duration: %.6g ps\n', time_ps(end));

%% ============================================================
% Isolate the requested resonance frequency (stable SOS filter)
% ============================================================

fs = 1 / dt;
nyquist_Hz = fs / 2;

if isempty(f0_Hz) || ~isfinite(f0_Hz) || f0_Hz <= 0
    error('Set f0_Hz to a positive finite target frequency.');
end

if ~isfinite(targetBandwidth_Hz) || targetBandwidth_Hz <= 0
    error('targetBandwidth_Hz must be positive and finite.');
end

if any(~isfinite(signal))
    error(['Input signal contains NaN or Inf. Check the monitor ' ...
           'file before filtering.']);
end

fLow_Hz  = f0_Hz - targetBandwidth_Hz / 2;
fHigh_Hz = f0_Hz + targetBandwidth_Hz / 2;

if fLow_Hz <= 0 || fHigh_Hz >= nyquist_Hz
    error('Filter band must lie between 0 and Nyquist.');
end

% Normalize the cutoff frequencies.
Wn = [fLow_Hz, fHigh_Hz] / nyquist_Hz;

% Design a Butterworth filter as second-order sections.
% SOS is more numerically stable than direct-form [b,a] coefficients.
[z, p, k] = butter(4, Wn, 'bandpass');
[sos, g] = zp2sos(z, p, k);

% Apply the overall gain to the first SOS section.
sos(1, 1:3) = g * sos(1, 1:3);

% Zero-phase filtering.
% Requires Signal Processing Toolbox.
filteredSignal = sosfilt(sos, signal);

if any(~isfinite(filteredSignal))
    error(['SOS filtering produced NaN or Inf. Check the input ' ...
           'signal and filter parameters.']);
end

fprintf('\n========== FREQUENCY ISOLATION ==========\n');
fprintf('Sampling frequency: %.6f PHz\n', fs / 1e15);
fprintf('Nyquist frequency:  %.6f PHz\n', nyquist_Hz / 1e15);
fprintf('Target frequency:   %.6f THz\n', f0_Hz / 1e12);
fprintf('Filter lower edge:  %.6f THz\n', fLow_Hz / 1e12);
fprintf('Filter upper edge:  %.6f THz\n', fHigh_Hz / 1e12);
fprintf('=========================================\n');

%% ============================================================
% Reject initial pulse
% ============================================================

figure('Color','w');
plot(time_ps, signal);
grid on;
xlabel('Time (ps)');
ylabel('Field amplitude');
title('Select the start of the ring-down');

if isempty(rejectBefore_ps)
disp('Click the plot at the time after the initial pulse.');
[xClick, ~] = ginput(1);
rejectBefore_ps = xClick;
end

if isempty(fitEnd_ps)
fitEnd_ps = time_ps(end);
end

if rejectBefore_ps < time_ps(1) || rejectBefore_ps >= fitEnd_ps
error('Invalid fitting interval.');
end

idx = time_ps >= rejectBefore_ps & time_ps <= fitEnd_ps;

tFit = time(idx);
xFit = filteredSignal(idx);

if numel(xFit) < 20
error('Too few samples in selected fitting interval.');
end

%% ============================================================
% Hilbert envelope
% ============================================================

% Remove DC offset before computing the analytic signal.
xFit = xFit - mean(xFit);

% Hilbert transform gives the analytic signal.
analyticSignal = hilbert(xFit);
envelope = abs(analyticSignal);

% Smooth the envelope lightly to reduce oscillatory ripple.
% The window should remain short compared with the decay time.
smoothSamples = max(1, round(0.5e-12 / dt));
smoothSamples = min(smoothSamples, max(1, floor(numel(envelope)/20)));

if smoothSamples > 1
envelope = movmean(envelope, smoothSamples);
end

%% ============================================================
% Select the envelope region to fit
% ============================================================

peakEnvelope = max(envelope);

if peakEnvelope <= 0
error('Envelope is zero throughout the selected interval.');
end

valid = envelope >= minimumEnvelopeFraction * peakEnvelope;

% Exclude points at or below the envelope threshold.
tFit = tFit(valid);
envelope = envelope(valid);

if numel(envelope) < 20
error('Too few envelope samples remain. Lower the envelope threshold.');
end

%% ============================================================
% Fit ln(envelope) = intercept + slope * time
% ============================================================

p = polyfit(tFit, log(envelope), 1);
slope = p(1);
intercept = p(2);

if slope >= 0
warning(['The fitted envelope is not decaying. Check source shutoff, ' ...
'gain, mode beating, and fitting interval.']);
tauA = NaN;
else
tauA = -1 / slope;
end

envelopeFit = exp(polyval(p, tFit));

% R-squared of the logarithmic envelope fit
logEnvelope = log(envelope);
logFit = polyval(p, tFit);

SSres = sum((logEnvelope - logFit).^2);
SStot = sum((logEnvelope - mean(logEnvelope)).^2);

if SStot > 0
R2 = 1 - SSres / SStot;
else
R2 = NaN;
end

%% ============================================================
% Calculate Q
% ============================================================

if isfinite(tauA) && f0_Hz > 0
Q = (pi * f0_Hz * tauA) / 2;
else
Q = NaN;
end

%% ============================================================
% Results
% ============================================================

fprintf('\n========== CAVITY RING-DOWN RESULTS ==========\n');
fprintf('Fitting start time:       %.6g ps\n', rejectBefore_ps);
fprintf('Fitting end time:         %.6g ps\n', fitEnd_ps);
fprintf('Resonant frequency:       %.6g THz\n', f0_Hz / 1e12);
fprintf('Amplitude decay tau:      %.6g ps\n', tauA * 1e12);
fprintf('Quality factor Q:         %.6g\n', Q);
fprintf('Log-envelope R^2:         %.6f\n', R2);
fprintf('==============================================\n');

%% ============================================================
% Plot field, envelope, and exponential fit
% ============================================================

figure('Color','w');

subplot(2,1,1);
plot(time_ps(idx), signal(idx), ...
    'Color', [0.75 0.75 0.75]);
hold on;
plot(time_ps(idx), filteredSignal(idx), 'b');
grid on;
xlabel('Time (ps)');
ylabel('Field amplitude');
title(sprintf('Ring-down near %.3f THz', f0_Hz/1e12));
legend('Original field', 'Filtered field');
hold off;

subplot(2,1,2);
semilogy(tFit * 1e12, envelope, 'b.');
hold on;
semilogy(tFit * 1e12, envelopeFit, 'r-', 'LineWidth', 1.5);
grid on;
xlabel('Time (ps)');
ylabel('Field envelope');
title(sprintf('Hilbert envelope fit: Q = %.5g, R^2 = %.5f', Q, R2));
legend('Hilbert envelope', 'Exponential fit', 'Location', 'best');
hold off;
