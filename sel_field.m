clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

dx0 = 2.033e-9;    % Spatial cell size (m)

% E-field component: 1 = Ex, 2 = Ey, 3 = Ez
component = 3;

% Optional physical position offset (m)
positionOffset = 0.0;

%% ============================================================
% Select vec3 file
% ============================================================

[file, path] = uigetfile('*.*', ...
'Select vec3 electric-field snapshot');

if isequal(file, 0)
return;
end

fid = fopen(fullfile(path, file), 'rb');

if fid == -1
error('Could not open file.');
end

data = fread(fid, 'single');
fclose(fid);

if mod(numel(data), 3) ~= 0
error('File does not contain a whole number of vec3 samples.');
end

%% ============================================================
% Extract electric field
% ============================================================

data = reshape(data, 3, []);

% Selected electric-field component
E = double(data(component, :).');

% Electric-field magnitude at each 1D cell
% Use this instead if the vector contains multiple nonzero components:
% E = sqrt(sum(double(data).^2, 1)).';

%% ============================================================
% Normalise to maximum absolute electric field
% ============================================================

Emax = max(abs(E));

if Emax == 0
warning('All electric-field values are zero.');
Enorm = E;
else
Enorm = E / Emax;
end

%% ============================================================
% Position vector
% ============================================================

N = numel(E);
x = positionOffset + (0:N-1).' * dx0;

%% ============================================================
% Report information
% ============================================================

fprintf('\n');
fprintf('============================================================\n');
fprintf('Electric-field spatial snapshot\n');
fprintf('============================================================\n');
fprintf('File                 : %s\n', file);
fprintf('Number of cells      : %d\n', N);
fprintf('Grid spacing         : %.6f nm\n', dx0*1e9);
fprintf('Domain length        : %.6f um\n', N*dx0*1e6);
fprintf('Selected component   : E%d\n', component);
fprintf('Maximum |E|          : %.6e\n', Emax);
fprintf('============================================================\n\n');

%% ============================================================
% Plot normalised field against position
% ============================================================

figure('Color', 'w');

plot(x*1e6, Enorm, 'LineWidth', 1.2);

xlabel('Position (\mum)');
ylabel(sprintf('Normalised E_%d / max|E_%d|', ...
component, component));

title('Normalised Electric Field Along 1D Cavity');

grid on;
%xlim([x(1), x(end)]*1e6);

ylim([-1.05, 1.05]);

%% ============================================================
% Optional: plot absolute field envelope
% ============================================================

figure('Color', 'w');

plot(x*1e6, abs(E)/max(Emax, eps), 'LineWidth', 1.2);

xlabel('Position (\mum)');
ylabel('Normalised |E|');

title('Normalised Electric-Field Magnitude Along Cavity');

grid on;
%xlim([x(1), x(end)]*1e6);
ylim([0, 1.05]);
