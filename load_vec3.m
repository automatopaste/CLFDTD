clear;
clc;
close all;

[file1,path1] = uigetfile('*.*','Select first file');
if isequal(file1,0), return; end

fid = fopen(fullfile(path1,file1),'rb');
A = fread(fid,'float32');
fclose(fid);

A = reshape(A,3,[])';
N = size(A,1);
t = 0:N-1;

figure('Position',[100 100 1400 900]);

labels = {'X','Y','Z'};

for c = 1:3

    subplot(4,2,2*c-1)

    plot(t,A(:,c),'b','LineWidth',1.5);

    grid on
    xlabel('Time step')
    ylabel(labels{c})
    title(sprintf('%s Component',labels{c}))
    legend('File 1','File 2')

end