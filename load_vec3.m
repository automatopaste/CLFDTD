matlab
clear;
clc;
close all;

[file1,path1] = uigetfile('*.*','Select data file');
if isequal(file1,0), return; end

fid = fopen(fullfile(path1,file1),'rb');
A = fread(fid,'float32');
fclose(fid);

% Data format: X Y Z X Y Z ...
A = reshape(A,3,[])';

N = size(A,1);
t = 0:N-1;

labels = {'X','Y','Z','Vector FFT Magnitude'};

%% ============================================================
% Moving-window FFT parameters
% =============================================================

windowLength = 2^14;    % Number of time samples in each FFT window
hopSize      = 16;      % Number of samples to move between windows

% Sampling interval
dt = 0.5 * 2.068e-16;   % seconds per simulation time step

% Optional frequency limits for plotting
fMin = -0.5 / dt;
fMax =  0.5 / dt;

% Hann window
window = hann(windowLength);

% Number of FFT points
nFFT = windowLength;

% Number of FFT windows
numWindows = floor((N - windowLength) / hopSize) + 1;

% Time associated with each FFT window
fftTime = zeros(1,numWindows);

% Frequency axis
f = (-nFFT/2:nFFT/2-1) / (nFFT * dt);

%% ============================================================
% Figure
% ============================================================

figure('Position',[100 100 1400 1200]);

for c = 1:4

    %% --------------------------------------------------------
    % Time-domain signal
    % ---------------------------------------------------------

    subplot(4,2,2*c-1);

    if c <= 3
        plot(t,A(:,c),'b','LineWidth',1.0);
    else
        % Vector field magnitude for the time-domain plot
        vectorMagnitude = sqrt( ...
            A(:,1).^2 + ...
            A(:,2).^2 + ...
            A(:,3).^2);

        plot(t,vectorMagnitude,'b','LineWidth',1.0);
    end

    grid on
    xlabel('Time step')
    ylabel(labels{c})
    title(sprintf('%s - Time Domain',labels{c}))

    %% --------------------------------------------------------
    % Moving-window FFT
    % ---------------------------------------------------------

    spectrogramData = zeros(nFFT,numWindows);

    for k = 1:numWindows

        % Start/end of this FFT window
        i0 = (k-1)*hopSize + 1;
        i1 = i0 + windowLength - 1;

        fftTime(k) = mean([t(i0), t(i1)]);

        if c <= 3

            %% ------------------------------------------------
            % Individual component FFT
            % -------------------------------------------------

            segment = A(i0:i1,c);

            % Remove DC component
            segment = segment - mean(segment);

            % Apply Hann window
            segment = segment .* window;

            % FFT
            F = fft(segment,nFFT);

            % Shift zero frequency to centre
            F = fftshift(F);

            % FFT magnitude
            magnitude = abs(F);

        else

            %% ------------------------------------------------
            % Vector FFT magnitude
            %
            % sqrt(|Fx|^2 + |Fy|^2 + |Fz|^2)
            % -------------------------------------------------

            segmentX = A(i0:i1,1);
            segmentY = A(i0:i1,2);
            segmentZ = A(i0:i1,3);

            % Remove DC component independently
            segmentX = segmentX - mean(segmentX);
            segmentY = segmentY - mean(segmentY);
            segmentZ = segmentZ - mean(segmentZ);

            % Apply identical Hann window
            segmentX = segmentX .* window;
            segmentY = segmentY .* window;
            segmentZ = segmentZ .* window;

            % FFT each component
            Fx = fftshift(fft(segmentX,nFFT));
            Fy = fftshift(fft(segmentY,nFFT));
            Fz = fftshift(fft(segmentZ,nFFT));

            % Vector magnitude in frequency domain
            magnitude = sqrt( ...
                abs(Fx).^2 + ...
                abs(Fy).^2 + ...
                abs(Fz).^2);
        end

        % Store FFT magnitude in dB
        spectrogramData(:,k) = ...
            20*log10(magnitude + 1e-12);

    end

    %% --------------------------------------------------------
    % Plot moving FFT
    % ---------------------------------------------------------

    subplot(4,2,2*c);

    imagesc(fftTime,f,spectrogramData);

    axis xy

    ylim([fMin fMax]);

    xlabel('Time step')
    ylabel('Frequency (Hz)')

    title(sprintf('%s - Moving FFT',labels{c}))

    colorbar

end

sgtitle(sprintf( ...
    'Moving-window FFT: Window = %d samples, Hop = %d samples', ...
    windowLength,hopSize));