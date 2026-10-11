clear;
clc;
close all;

%% ============================================================
% Configuration
% ============================================================

% Grid dimensions (number of cells in each axis)
nx = input('Grid size X: ');
ny = input('Grid size Y: ');
nz = input('Grid size Z: ');

% Physical cell spacing, in metres
dx0 = 1.2395543175e-5 / 800.0;
dy0 = dx0;
dz0 = dx0;

% Isosurface levels of the normalized field
contourLevels = [-1.0, -0.5, -0.2, 0.2, 0.5, 1.0];

% Component labels
componentLabels = {'E_x', 'E_y', 'E_z'};

%% ============================================================
% Load binary vec3 snapshot
% ============================================================

[file, path] = uigetfile( ...
{'*.bin;*.dat;*.*', 'Binary field files'}, ...
'Select 3D electric field snapshot');

if isequal(file, 0)
disp('No file selected.');
return;
end

filename = fullfile(path, file);

fid = fopen(filename, 'rb');
if fid == -1
error('Could not open file: %s', filename);
end

data = fread(fid, [3, inf], '3*single=>double');
fclose(fid);

expectedCells = nx * ny * nz;
actualCells = size(data, 2);

if actualCells ~= expectedCells
error(['File contains %d vec3 samples, but grid dimensions ' ...
'require %d (%d x %d x %d).'], ...
actualCells, expectedCells, nx, ny, nz);
end

%% ============================================================
% Reconstruct all three field components
%
% Binary data layout: vec3 per cell, with three consecutive
% float32 values: Ex, Ey, Ez.
%
% Linear index (0-based):
%   index = ix + nx*iy + nx*ny*iz
%
% MATLAB reshape preserves the equivalent X-fastest layout.
% ============================================================

Ex3D = reshape(data(1, :).', [nx, ny, nz]);
Ey3D = reshape(data(2, :).', [nx, ny, nz]);
Ez3D = reshape(data(3, :).', [nx, ny, nz]);

fields = {Ex3D, Ey3D, Ez3D};

%% ============================================================
% Coordinates in micrometres
% ============================================================

x = (0:nx-1) * dx0 * 1e6;
y = (0:ny-1) * dy0 * 1e6;
z = (0:nz-1) * dz0 * 1e6;

%% ============================================================
% Generate separate isosurface plots for Ex, Ey and Ez
% ============================================================

colors = parula(numel(contourLevels));

fprintf('Loaded: %s\n', filename);
fprintf('Grid: %d x %d x %d\n', nx, ny, nz);

for component = 1:3

%% Select component and normalize independently
E3D = fields{component};

maxField = max(abs(E3D(:)));

fprintf('%s maximum absolute field: %.6g\n', ...
    componentLabels{component}, maxField);

if maxField > 0
    E3D = E3D / maxField;
else
    warning('%s is zero everywhere.', ...
        componentLabels{component});
end

%% Create a separate figure
figure('Color', 'w', ...
    'Name', componentLabels{component});

hold on;

%% Plot positive and negative isosurfaces
for k = 1:numel(contourLevels)

    level = contourLevels(k);
    surfaceData = isosurface(E3D, level);

    if ~isempty(surfaceData.vertices)

        % Convert vertices from grid indices to micrometres
        surfaceData.vertices(:, 1) = ...
            x(1) + (surfaceData.vertices(:, 1) - 1) ...
            * dx0 * 1e6;

        surfaceData.vertices(:, 2) = ...
            y(1) + (surfaceData.vertices(:, 2) - 1) ...
            * dy0 * 1e6;

        surfaceData.vertices(:, 3) = ...
            z(1) + (surfaceData.vertices(:, 3) - 1) ...
            * dz0 * 1e6;

        patch(surfaceData, ...
            'FaceColor', colors(k, :), ...
            'EdgeColor', 'none', ...
            'FaceAlpha', 0.35);
    end
end

%% Labels and plot formatting
xlabel('X (\mum)');
ylabel('Y (\mum)');
zlabel('Z (\mum)');

title(sprintf('Normalized Electric Field %s', ...
    componentLabels{component}), ...
    'Interpreter', 'tex');

axis tight;
axis equal;
grid on;
box on;
view(3);

camlight headlight;
lighting gouraud;

colormap(parula);
cb = colorbar;
cb.Label.String = 'Normalized field level';

% Show the normalized contour levels on the colorbar
clim([min(contourLevels), max(contourLevels)]);

hold off;

end
