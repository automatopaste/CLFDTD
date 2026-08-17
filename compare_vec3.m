%% Compare two vec3 time histories

clear;
clc;
close all;

%% Select files

[file1,path1] = uigetfile('*.*','Select first file');
if isequal(file1,0), return; end

[file2,path2] = uigetfile('*.*','Select second file');
if isequal(file2,0), return; end

%% Read data

fid = fopen(fullfile(path1,file1),'rb');
A = fread(fid,'float32');
fclose(fid);

fid = fopen(fullfile(path2,file2),'rb');
B = fread(fid,'float32');
fclose(fid);

%% Modify

%% Validate

assert(mod(numel(A),3)==0,'First file is not a vec3 array.');
assert(mod(numel(B),3)==0,'Second file is not a vec3 array.');

A = reshape(A,3,[])';
B = resize(reshape(B,3,[])', size(A));

N = size(A,1);
t = 0:N-1;

%% Compute errors

D = B - A;

magA = vecnorm(A,2,2);
magB = vecnorm(B,2,2);

magErr = vecnorm(D,2,2);

%% Statistics

fprintf("Timesteps           : %d\n",N);
fprintf("RMSE                : %.8g\n",sqrt(mean(D(:).^2)));
fprintf("Mean vector error   : %.8g\n",mean(magErr));
fprintf("Maximum vector error: %.8g\n",max(magErr));

%% Plot

figure('Position',[100 100 1400 900]);

labels = {'X','Y','Z'};

for c = 1:3

    subplot(4,2,2*c-1)

    plot(t,A(:,c),'b','LineWidth',1.5);
    hold on;
    plot(t,B(:,c),'r--','LineWidth',1.5);
    hold off;

    grid on
    xlabel('Time step')
    ylabel(labels{c})
    title(sprintf('%s Component',labels{c}))
    legend('File 1','File 2')

    subplot(4,2,2*c)

    plot(t,D(:,c),'k');
    grid on
    xlabel('Time step')
    ylabel(sprintf('\x0394%s',labels{c}))
    title(sprintf('%s Error',labels{c}))

end

figure('Position',[200 200 1200 700]);

subplot(2,1,1)

plot(t,magA,'b','LineWidth',1.5)
hold on
plot(t,magB,'r--','LineWidth',1.5)
hold off

grid on
xlabel('Time step')
ylabel('|v|')
title('Vector Magnitude')
legend('File 1','File 2')

subplot(2,1,2)

plot(t,magErr,'k','LineWidth',1.5)
grid on
xlabel('Time step')
ylabel('|Δ|')
title('Vector Error Magnitude')

%% Largest errors

[~,idx] = sort(magErr,'descend');

fprintf('\nLargest errors:\n');
fprintf('Step\t|Δ|\t\tFile1\t\t\t\tFile2\n');

for i = 1:min(10,N)

    k = idx(i);

    fprintf('%5d\t%.6g\t[% .5g % .5g % .5g]\t[% .5g % .5g % .5g]\n',...
        k,...
        magErr(k),...
        A(k,1),A(k,2),A(k,3),...
        B(k,1),B(k,2),B(k,3));

end