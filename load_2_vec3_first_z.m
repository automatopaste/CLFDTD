clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

c0 = 299792458;          % m/s

% FDTD spatial discretisation
dx0 = 1e-6 / 50.0;       % m
courant = 0.5;
dt = courant * dx0 / c0; % s per timestep

%% ============================================================
% Time scaling for each file
% ============================================================
%
% Set these to the actual time between consecutive samples.
%
% Meep file:
%   One float32 value per sample (Ez).
%
% Simulation file:
%   Three packed float32 values per sample (X, Y, Z).
%
% If both files record every timestep:
%   dt1 = dt;
%   dt2 = dt;
%
% If Meep uses a different sampling interval, set dt1
% to that interval in seconds.

dt1 = dt;  % Meep: seconds/sample
dt2 = dt;  % Your simulation: seconds/sample

%% ============================================================
% Select Meep binary file (scalar float32)
% ============================================================

[file1, path1] = uigetfile( ...
    '*.*', 'Select MEEP scalar Ez binary file');

if isequal(file1, 0)
    return;
end

fid = fopen(fullfile(path1, file1), 'rb', 'ieee-le');
if fid == -1
    error('Could not open Meep file.');
end

A = fread(fid, Inf, 'float32=>single');
fclose(fid);

A = double(A(:));

%% ============================================================
% Select simulation binary file (packed vec3 float32)
% ============================================================

[file2, path2] = uigetfile( ...
    '*.*', 'Select simulation packed XYZ binary file');

if isequal(file2, 0)
    return;
end

fid = fopen(fullfile(path2, file2), 'rb', 'ieee-le');
if fid == -1
    error('Could not open simulation file.');
end

Braw = fread(fid, Inf, 'float32=>single');
fclose(fid);

if mod(numel(Braw), 3) ~= 0
    error(['Simulation file does not contain a whole number ' ...
           'of XYZ samples. Float count = %d'], numel(Braw));
end

% Each sample is stored as X, Y, Z.
Bvec = reshape(Braw, 3, []).';

% Extract Z only.
Bz = double(Bvec(:, 3));

%% ============================================================
% File lengths
% ============================================================

N1 = numel(A);
N2 = numel(Bz);

fprintf('\nMeep file: %s\n', file1);
fprintf('Simulation file: %s\n', file2);

fprintf('Meep scalar samples: %d\n', N1);
fprintf('Simulation vec3 samples: %d\n', N2);

fprintf('Meep dt: %.12g s\n', dt1);
fprintf('Simulation dt: %.12g s\n', dt2);

fprintf('Meep duration: %.12g s\n', (N1-1)*dt1);
fprintf('Simulation duration: %.12g s\n', (N2-1)*dt2);

%% ============================================================
% Time offset
% ============================================================
%
% Positive offset shifts the simulation data later.
% Negative offset shifts it earlier.

timeOffset = 0.0; % seconds

%% ============================================================
% Create time axes
% ============================================================

tA = (0:N1-1)' * dt1;
tB = (0:N2-1)' * dt2 + timeOffset;

%% ============================================================
% Plot Z-component comparison
% ============================================================

figure('Position', [100 100 1400 700]);

plot(tA * 1e12, A, 'LineWidth', 1.0);
hold on;

plot(tB * 1e12, Bz, 'LineWidth', 1.0);

grid on;
xlabel('Time (ps)');
ylabel('E_z (file units)');
title('Meep E_z vs Simulation E_z');
legend(file1, file2, ...
    'Interpreter', 'none', ...
    'Location', 'best');

%% ============================================================
% Optional: compare normalized waveforms
% ============================================================

figure('Position', [100 100 1400 700]);

scaleA = max(abs(A));
scaleB = max(abs(Bz));

if scaleA > 0
    plot(tA * 1e12, A / scaleA, 'LineWidth', 1.0);
    hold on;
end

if scaleB > 0
    plot(tB * 1e12, Bz / scaleB, 'LineWidth', 1.0);
    hold on;
end

grid on;
xlabel('Time (ps)');
ylabel('Normalized E_z');
title('Normalized Waveform Comparison');
legend(file1, file2, ...
    'Interpreter', 'none', ...
    'Location', 'best');

%% ============================================================
% Optional: export extracted simulation Z component
% ============================================================

[saveFile, savePath] = uiputfile( ...
    '*.bin', 'Save extracted Z-component binary', ...
    'simulation_Ez.bin');

if ~isequal(saveFile, 0)
    fid = fopen(fullfile(savePath, saveFile), 'wb', 'ieee-le');
    if fid == -1
        error('Could not create output binary file.');
    end

    fwrite(fid, single(Bz), 'float32');
    fclose(fid);

    fprintf('Extracted Z component saved to: %s\n', ...
        fullfile(savePath, saveFile));
end