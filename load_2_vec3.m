clear;
clc;
close all;

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
% Verify identical lengths
% ============================================================

N1 = size(A, 1);
N2 = size(B, 1);

if N1 ~= N2
    error(['Files must have identical lengths.\n' ...
           'File 1: %d vec3s\n' ...
           'File 2: %d vec3s'], N1, N2);
end

N = N1;

fprintf('File 1: %s\n', file1);
fprintf('File 2: %s\n', file2);
fprintf('Number of vec3 samples: %d\n', N);

%% ============================================================
% Time shift
% ============================================================

% Number of vec3 samples to shift File 2.
%
% Positive:
%   File 2 appears later in time (shift right)
%
% Negative:
%   File 2 appears earlier in time (shift left)
%
sampleOffset = -8;

%% ============================================================
% Create shifted version of File 2
% ============================================================

B_shifted = NaN(size(B));

if sampleOffset > 0

    % File 2 shifted RIGHT
    %
    % Original:
    % B(1) B(2) B(3) ...
    %
    % Shifted:
    % NaN ... NaN B(1) B(2) B(3) ...

    B_shifted(sampleOffset+1:end, :) = ...
        B(1:end-sampleOffset, :);

elseif sampleOffset < 0

    % File 2 shifted LEFT

    shift = -sampleOffset;

    B_shifted(1:end-shift, :) = ...
        B(shift+1:end, :);

else

    % No shift
    B_shifted = B;

end

%% ============================================================
% Sample axis
% ============================================================

t = 0:N-1;

%% ============================================================
% Vector magnitudes
% ============================================================

magnitudeA = sqrt( ...
    A(:,1).^2 + ...
    A(:,2).^2 + ...
    A(:,3).^2);

magnitudeB = sqrt( ...
    B_shifted(:,1).^2 + ...
    B_shifted(:,2).^2 + ...
    B_shifted(:,3).^2);

%% ============================================================
% Plot individual components and vector magnitude
% ============================================================

figure('Position', [100 100 1600 1000]);

labels = {'X', 'Y', 'Z', 'Vector Magnitude'};

for c = 1:4

    subplot(4,1,c);

    if c <= 3

        plot(t, A(:,c), 'LineWidth', 1.0);
        hold on;

        plot(t, B_shifted(:,c), 'LineWidth', 1.0);

    else

        plot(t, magnitudeA, 'LineWidth', 1.0);
        hold on;

        plot(t, magnitudeB, 'LineWidth', 1.0);

    end

    grid on;

    ylabel(labels{c});

    if c == 4
        xlabel('Vec3 sample');
    end

    title(sprintf('%s Comparison', labels{c}));

end

legend( ...
    file1, ...
    sprintf('%s (shift = %+d)', file2, sampleOffset), ...
    'Interpreter', 'none', ...
    'Location', 'best');

sgtitle(sprintf( ...
    'Raw Vec3 Comparison — File 2 shifted by %+d vec3 samples', ...
    sampleOffset));

%% ============================================================
% Plot difference
% ============================================================

difference = B_shifted - A;

figure('Position', [150 150 1600 900]);

subplot(4,1,1);
plot(t, difference(:,1), 'LineWidth', 1.0);
grid on;
ylabel('\Delta X');
title('File 2 - File 1');

subplot(4,1,2);
plot(t, difference(:,2), 'LineWidth', 1.0);
grid on;
ylabel('\Delta Y');

subplot(4,1,3);
plot(t, difference(:,3), 'LineWidth', 1.0);
grid on;
ylabel('\Delta Z');

differenceMagnitude = sqrt( ...
    difference(:,1).^2 + ...
    difference(:,2).^2 + ...
    difference(:,3).^2);

subplot(4,1,4);
plot(t, differenceMagnitude, 'LineWidth', 1.0);
grid on;
ylabel('|\Delta|');
xlabel('Vec3 sample');

sgtitle(sprintf( ...
    'Difference — File 2 shifted by %+d vec3 samples', ...
    sampleOffset));
