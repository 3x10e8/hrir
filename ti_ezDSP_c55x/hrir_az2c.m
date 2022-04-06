% Originally posted on October 20, 2015:
% https://diyrecorder.wordpress.com/2015/10/20/10000-tap-headers-slow-implementations-and-distortion/

function hrir_az2c(file, arrName, hrir, el)
%// arrName: name for the C array that will be created, could use a string corresponding to the MATLAB array:
%// hrir: the hrir 25x50x200 array
%// el: current plan is to output one header per elevation per L/R channel
%// Adapted from Aj's post here: 
%// http://www.mathworks.com/matlabcentral/newsreader/view_thread/48182
 
scale = 1e3; %// weights are very small -> round to 0s
[max_el, max_az, max_tap] = size(hrir);
if (el>max_el)
    el = max_el;
end
toWrite = ['Int16 ', arrName, '[', num2str(max_az*max_tap), '] = {'];
for az = 1:max_az
    for tap = 1:max_tap
        if az ~= max_az && tap ~= max_tap
            toWrite = [ toWrite, sprintf('%i, ', int16(scale.*hrir(el, az, tap)))];
        end
    end
end
toWrite = [toWrite, sprintf('%i};', int16(scale.*hrir(el, max_az, max_tap)))];
 
fid = fopen([file, '.h'], 'w');
fprintf(fid, toWrite);
fclose(fid);
end