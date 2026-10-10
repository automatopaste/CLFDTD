clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

c0 = 299792458;

% Use the exact spatial and temporal discretisation from FDTD.
dx0 = 1.363509749e-6 / 400.0;
courant = 0.5;
dt = courant * dx0 / c0;

% File format:
%   'binary' : raw float32 values
%   'text'   : text file with numeric columns
fileFormat = 'binary';

% Binary layout:
%   1 = one scalar sample per timestep
%   3 = interleaved Ex,Ey,Ez per timestep
componentsPerSample = 3;

% For 3-component files, analyse one component (1=Ex, 2=Ey, 3=Ez).
% Prefer a component with a strong signal at the cavity resonance.
componentToAnalyse = 3;

% Fit the exponential envelope over this time interval.
% Adjust these after inspecting the plotted envelopes.
fitStartTime = 0.0;       % seconds
fitEndTime   = 0.40e-9;   % seconds

% Envelope processing
smoothWindowCycles = 5;   % moving-average window in optical cycles
latePlateauFraction = 0.10;
saturationFraction = 0.90;

%% ============================================================
% Select files and associate each file with its gain
% ============================================================

[fileNames, folder] = uigetfile( ...
    {'*.bin;*.dat;*.raw;*.txt;*.csv', ...
     'Field data files (*.bin, *.dat, *.raw, *.txt, *.csv)'}, ...
    'Select field files for all gain values', ...
    'MultiSelect', 'on');

if isequal(fileNames, 0)
    disp('No files selected.');
    return;
end

if ischar(fileNames)
    fileNames = {fileNames};
end

numRuns = numel(fileNames);

fprintf('\nSelected files:\n');
for k = 1:numRuns
    fprintf('%2d: %s\n', k, fileNames{k});
end

% Enter one signed sigma0 value per file, in the order listed above.
% Example: [-10000 -15000 -20000 -25000 -30000]
gainValues = input( ...
    '\nEnter signed sigma0 values in the listed file order: ');

if numel(gainValues) ~= numRuns
    error('You must enter exactly one gain value per selected file.');
end

gainValues = gainValues(:);

% Sort by gain for plotting and threshold interpolation.
[gainValues, sortIdx] = sort(gainValues);
fileNames = fileNames(sortIdx);

%% ============================================================
% Read the data and calculate the field envelope
% ============================================================

time = cell(numRuns, 1);
field = cell(numRuns, 1);
envelope = cell(numRuns, 1);

growthRate = nan(numRuns, 1);
fitR2 = nan(numRuns, 1);
saturationTime = nan(numRuns, 1);
plateauAmplitude = nan(numRuns, 1);

for k = 1:numRuns

    filePath = fullfile(folder, fileNames{k});

    % --------------------------------------------------------
    % Read field samples
    % --------------------------------------------------------
    switch lower(fileFormat)
        case 'binary'
            fid = fopen(filePath, 'rb');
            if fid < 0
                error('Could not open file: %s', filePath);
            end
            raw = fread(fid, inf, 'float32=>double');
            fclose(fid);

        case 'text'
            raw = readmatrix(filePath);
            raw = raw(:);
            raw = raw(isfinite(raw));

        otherwise
            error('fileFormat must be binary or text.');
    end

    if componentsPerSample == 3
        if mod(numel(raw), 3) ~= 0
            error(['File %s does not contain a multiple of 3 ' ...
                   'samples. Check the binary layout.'], fileNames{k});
        end

        samples = reshape(raw, 3, []).';
        E = samples(:, componentToAnalyse);

    elseif componentsPerSample == 1
        E = raw;

    else
        error('componentsPerSample must be 1 or 3.');
    end

    E = E(:);

    if numel(E) < 100
        error('Too few samples in %s.', fileNames{k});
    end

    if any(~isfinite(E))
        error('NaN or Inf found in %s.', fileNames{k});
    end

    N = numel(E);
    t = (0:N-1)' * dt;

    % --------------------------------------------------------
    % Extract the amplitude envelope
    % --------------------------------------------------------
    % The Hilbert transform separates the carrier oscillation
    % from its slowly varying amplitude.
    env = abs(hilbert(E));

    % Smooth over several optical cycles to suppress residual
    % carrier ripple in the envelope.
    %
    % This estimate is for plotting and saturation detection.
    % The fitting interval should avoid the initial transient.
    fReference = 337e12;
    smoothSamples = max(1, round( ...
        smoothWindowCycles / (fReference * dt)));

    if smoothSamples > 1
        env = movmean(env, smoothSamples);
    end

    time{k} = t;
    field{k} = E;
    envelope{k} = env;

    % --------------------------------------------------------
    % Fit exponential growth/decay:
    %
    % envelope(t) ~ A * exp(growthRate * t)
    %
    % Therefore growthRate = slope(log(envelope)).
    % Positive = growing, negative = decaying.
    % --------------------------------------------------------
    fitMask = t >= fitStartTime & ...
              t <= fitEndTime & ...
              env > 0 & isfinite(env);

    if nnz(fitMask) < 20
        warning(['Not enough samples in the fitting interval ' ...
                 'for %s. Adjust fitStartTime / fitEndTime.'], ...
                 fileNames{k});
    else
        p = polyfit(t(fitMask), log(env(fitMask)), 1);
        growthRate(k) = p(1);

        logFit = polyval(p, t(fitMask));
        logData = log(env(fitMask));

        ssRes = sum((logData - logFit).^2);
        ssTot = sum((logData - mean(logData)).^2);
        fitR2(k) = 1 - ssRes / max(ssTot, eps);
    end

    % --------------------------------------------------------
    % Estimate saturation time.
    %
    % Estimate the late-time plateau from the median envelope
    % in the last portion of the run. Saturation is the first
    % time the envelope reaches a specified fraction of that
    % plateau, after the exponential fitting interval.
    %
    % This is meaningful primarily for growing, above-threshold
    % runs. A finite run that has not plateaued cannot establish
    % a saturation time.
    % --------------------------------------------------------
    nLate = max(10, round(latePlateauFraction * N));
    nLate = min(nLate, N);

    lateEnv = env(end-nLate+1:end);
    plateau = median(lateEnv);
    plateauAmplitude(k) = plateau;

    % Require an actual growth trend before reporting a time.
    if isfinite(growthRate(k)) && growthRate(k) > 0 && plateau > 0
        searchMask = t >= fitEndTime & ...
                     env >= saturationFraction * plateau;

        idx = find(searchMask, 1, 'first');

        % Reject the result if the final envelope is still
        % substantially below its estimated plateau, or if
        % the signal is still growing strongly at the end.
        tailStart = max(1, N - nLate + 1);
        tailTime = t(tailStart:end);
        tailLogEnv = log(max(env(tailStart:end), realmin));

        if numel(tailTime) >= 20
            pTail = polyfit(tailTime, tailLogEnv, 1);
            tailRate = pTail(1);
        else
            tailRate = NaN;
        end

        plateauVariation = std(lateEnv) / max(plateau, eps);

        if ~isempty(idx) && ...
                isfinite(tailRate) && ...
                abs(tailRate) < 0.10 * max(growthRate(k), eps) && ...
                plateauVariation < 0.30
            saturationTime(k) = t(idx);
        end
    end

    fprintf('\n%s\n', fileNames{k});
    fprintf('  sigma0:            %.6g S/m\n', gainValues(k));
    fprintf('  Growth rate:       %.6g 1/s\n', growthRate(k));
    fprintf('  Fit R^2:           %.5f\n', fitR2(k));

    if isfinite(saturationTime(k))
        fprintf('  Estimated plateau time: %.6g s\n', saturationTime(k));
    else
        fprintf('  Saturation time:   not reliably detected\n');
    end
end

%% ============================================================
% Plot the field envelopes
% ============================================================

figure('Name', 'Cavity field envelopes', 'Color', 'w');
hold on;
grid on;
box on;

colors = lines(numRuns);

for k = 1:numRuns
    plot(time{k} * 1e9, envelope{k}, ...
        'Color', colors(k,:), ...
        'LineWidth', 1.3, ...
        'DisplayName', sprintf('\\sigma_0 = %.4g S/m', gainValues(k)));
end

xlabel('Time (ns)');
ylabel('Electric-field envelope (V/m, or simulation units)');
title('Cavity field envelope for different gain values');
legend('Location', 'best', 'Interpreter', 'tex');

%% ============================================================
% Plot the exponential growth rate against gain
% ============================================================

figure('Name', 'Lasing threshold estimate', 'Color', 'w');
hold on;
grid on;
box on;

valid = isfinite(growthRate);

plot(gainValues(valid), growthRate(valid) * 1e-9, ...
    'o-', 'LineWidth', 1.5, 'MarkerSize', 7);

yline(0, 'k--', 'Zero growth', 'LineWidth', 1.2);

xlabel('Signed gain conductivity \sigma_0 (S/m)');
ylabel('Envelope growth rate (ns^{-1})');
title('Lasing threshold from exponential growth rate');

%% ============================================================
% Estimate threshold by interpolation at zero growth
% ============================================================

thresholdGain = NaN;
crossingFound = false;

validIdx = find(valid);

for j = 1:numel(validIdx)-1
    i1 = validIdx(j);
    i2 = validIdx(j+1);

    r1 = growthRate(i1);
    r2 = growthRate(i2);

    if r1 == 0
        thresholdGain = gainValues(i1);
        crossingFound = true;
        break;
    elseif r1 * r2 < 0
        % Linear interpolation between the two neighbouring
        % gain values that straddle zero growth.
        thresholdGain = gainValues(i1) + ...
            (0 - r1) * (gainValues(i2) - gainValues(i1)) / ...
            (r2 - r1);

        crossingFound = true;
        break;
    end
end

if crossingFound
    fprintf('\nEstimated threshold conductivity: sigma0 = %.8g S/m\n', ...
        thresholdGain);

    xline(thresholdGain, 'r--', ...
        sprintf('Threshold: %.4g S/m', thresholdGain), ...
        'LineWidth', 1.3);
else
    fprintf(['\nNo zero-growth crossing was found. ' ...
             'Run more simulations with gain values bracketing ' ...
             'the transition from decay to growth.\n']);
end

%% ============================================================
% Plot estimated saturation times
% ============================================================

figure('Name', 'Cavity saturation time', 'Color', 'w');
hold on;
grid on;
box on;

satValid = isfinite(saturationTime);

if any(satValid)
    plot(gainValues(satValid), saturationTime(satValid) * 1e9, ...
        'o-', 'LineWidth', 1.5, 'MarkerSize', 7);
end

xlabel('Signed gain conductivity \sigma_0 (S/m)');
ylabel('Estimated saturation time (ns)');
title('Time to reach the saturated field plateau');

%% ============================================================
% Summary table
% ============================================================

results = table( ...
    string(fileNames(:)), ...
    gainValues, ...
    growthRate, ...
    fitR2, ...
    saturationTime, ...
    plateauAmplitude, ...
    'VariableNames', { ...
        'File', 'Sigma0_S_per_m', 'GrowthRate_per_s', ...
        'FitR2', 'SaturationTime_s', 'LatePlateauAmplitude'});

disp(results);
