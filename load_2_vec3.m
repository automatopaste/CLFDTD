clear;
clc;
close all;

%% ============================================================
% Time scaling for each file
% ============================================================
%
% Time between consecutive vec3 samples.
%
% Example:
%   dt1 = 0.5;     % File 1: 0.5 simulation time units/sample
%   dt2 = 0.25;    % File 2: 0.25 simulation time units/sample
%
% These can be different.

dt1 = 1.0;
dt2 = 1.0;

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

A = fread(fid, 'float32');
fclose(fid);

% Data format:
% X Y Z X Y Z ...

if mod(numel(A), 3) ~= 0
    error('First file does not contain a whole number of vec3 samples.');
end

A = reshape(A, 3, [])';

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

B = fread(fid, 'float32');
fclose(fid);

if mod(numel(B), 3) ~= 0
    error('Second file does not contain a whole number of vec3 samples.');
end

B = reshape(B, 3, [])';

%% ============================================================
% File lengths
% ============================================================

N1 = size(A, 1);
N2 = size(B, 1);

fprintf('File 1: %s\n', file1);
fprintf('File 2: %s\n', file2);

fprintf('File 1 samples: %d\n', N1);
fprintf('File 2 samples: %d\n', N2);

fprintf('File 1 dt: %.12g\n', dt1);
fprintf('File 2 dt: %.12g\n', dt2);

fprintf('File 1 duration: %.12g\n', (N1-1)*dt1);
fprintf('File 2 duration: %.12g\n', (N2-1)*dt2);

%% ============================================================
% Time shift
% ============================================================
%
% Physical time shift applied to File 2.
%
% Positive:
%   File 2 appears later in time
%
% Negative:
%   File 2 appears earlier in time
%
% This is now expressed in simulation time units rather than
% number of samples.
%
timeOffset = 0.0;

%% ============================================================
% Create time axes
% ============================================================

tA = (0:N1-1)' * dt1;
tB = (0:N2-1)' * dt2 + timeOffset;

%% ============================================================
% Vector magnitudes
% ============================================================

magnitudeA = sqrt( ...
    A(:,1).^2 + ...
    A(:,2).^2 + ...
    A(:,3).^2);

magnitudeB = sqrt( ...
    B(:,1).^2 + ...
    B(:,2).^2 + ...
    B(:,3).^2);

%% ============================================================
% Plot individual components and vector magnitude
% ============================================================

figure('Position', [100 100 1600 1000]);

labels = {'X', 'Y', 'Z', 'Vector Magnitude'};

for c = 1:4

    subplot(4,1,c);

    if c <= 3

        plot(tA, A(:,c), 'LineWidth', 1.0);
        hold on;

        plot(tB, B(:,c), 'LineWidth', 1.0);

    else

        plot(tA, magnitudeA, 'LineWidth', 1.0);
        hold on;

        plot(tB, magnitudeB, 'LineWidth', 1.0);

    end

    grid on;

    ylabel(labels{c});

    if c == 4
        xlabel('Simulation time');
    end

    title(sprintf('%s Comparison', labels{c}));

end

legend( ...
    file1, ...
    sprintf('%s (\\Delta t = %+g)', file2, timeOffset), ...
    'Interpreter', 'none', ...
    'Location', 'best');

sgtitle(sprintf( ...
    'Raw Vec3 Comparison — File 2 time shifted by %+g', ...
    timeOffset));

%% ============================================================
% Common time interval
% ============================================================
%
% Only compare where BOTH files contain data.
%
tStart = max(min(tA), min(tB));
tEnd   = min(max(tA), max(tB));

if tStart >= tEnd
    error('Files have no overlapping time interval.');
end

%% ============================================================
% Interpolate File 2 onto File 1 time samples
% ============================================================
%
% This allows the files to:
%
%   - have different lengths
%   - have different dt
%   - have a physical time offset
%
% File 1 remains on its native time axis.

validA = (tA >= tStart) & (tA <= tEnd);

tCommon = tA(validA);

B_interp = zeros(numel(tCommon), 3);

for c = 1:3
    B_interp(:,c) = interp1( ...
        tB, ...
        B(:,c), ...
        tCommon, ...
        'linear');
end

A_common = A(validA, :);

%% ============================================================
% Difference
% ============================================================

difference = B_interp - A_common;

differenceMagnitude = sqrt( ...
    difference(:,1).^2 + ...
    difference(:,2).^2 + ...
    difference(:,3).^2);

%% ============================================================
% Plot difference
% ============================================================

figure('Position', [150 150 1600 900]);

subplot(4,1,1);
plot(tCommon, difference(:,1), 'LineWidth', 1.0);
grid on;
ylabel('\Delta X');
title('File 2 - File 1');

subplot(4,1,2);
plot(tCommon, difference(:,2), 'LineWidth', 1.0);
grid on;
ylabel('\Delta Y');

subplot(4,1,3);
plot(tCommon, difference(:,3), 'LineWidth', 1.0);
grid on;
ylabel('\Delta Z');

subplot(4,1,4);
plot(tCommon, differenceMagnitude, 'LineWidth', 1.0);
grid on;
ylabel('|\Delta|');
xlabel('Simulation time');

sgtitle(sprintf( ...
    'Difference — File 2 shifted by %+g time units', ...
    timeOffset));

%% ============================================================
% Print comparison information
% ============================================================

fprintf('\n');
fprintf('Common time interval:\n');
fprintf('  Start: %.12g\n', tStart);
fprintf('  End:   %.12g\n', tEnd);
fprintf('  Duration: %.12g\n', tEnd - tStart);
fprintf('  Comparison samples: %d\n', numel(tCommon));