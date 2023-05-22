clear;clc;
gt_data = 'CASIA-distance';
% ����pupil_boundary_circle_edge��iris_boundary_circle_edge�ļ��е�·��
method1_folder= 'h:\tifs_paper\comparison_mask_methods\method1\CASIA-distance\';  
method2_folder= 'h:\tifs_paper\comparison_mask_methods\method2\CASIA-distance\';
method3_folder= 'h:\tifs_paper\comparison_mask_methods\method3\CASIA-distance\';
proposed_folder= 'h:\tifs_paper\comparison_mask_methods\proposed\CASIA-distance\';
 

algo_set={method1_folder,method2_folder,method3_folder,proposed_folder};

%ָ����Ĥ����߽��gt·��
switch(gt_data)
    case 'CASIA-distance'
        iris_gt_dir = 'h:\pr_data\segmentation_localization\CASIA-distance\test\iris_edge\';
        pupil_gt_dir = 'h:\pr_data\segmentation_localization\CASIA-distance\test\pupil_edge\';
    case 'miche'
        iris_gt_dir = 'h:\pr_data\segmentation_localization\MICHE\test\iris_edge\';
        pupil_gt_dir = 'h:\pr_data\segmentation_localization\MICHE\test\pupil_edge\';
    case 'nice1'
        iris_gt_dir = 'h:\pr_data\segmentation_localization\NICE\NICE1\test\iris_edge\';
        pupil_gt_dir = 'h:\pr_data\segmentation_localization\NICE\NICE1\test\pupil_edge\';
    otherwise
        disp('error')
end

files=dir([pupil_gt_dir,'*.png']);
n = length(files);
m=length(algo_set);
hist_dist_v=zeros(16,m);


for j=1:n
    [filename, type] = strtok(files(j).name, '.');
    pupil_gt = imread([pupil_gt_dir,files(j).name]);
    pupil_gt = pupil_gt(:, :, 1);  %ȷ���ǻҶ�ͼ��
    iris_gt = imread([iris_gt_dir,files(j).name]);
    iris_gt = iris_gt(:, :, 1);  %ȷ���ǻҶ�ͼ��
    
    for i=1:m
        pupil_algo_filename=strcat(algo_set{i},'pupil_boundary_circle_edge\',filename,'.png');
        iris_algo_filename=strcat(algo_set{i},'iris_boundary_circle_edge\',filename,'.png');
        pupil = imread(pupil_algo_filename);
        iris = imread(iris_algo_filename);
        pupil_Hausdorff= Hausdorff(pupil,pupil_gt);
        iris_Hausdorff= Hausdorff(iris,iris_gt);
        if( iris_Hausdorff ~= Inf && pupil_Hausdorff ~=Inf)
            for k=1:16
                if iris_Hausdorff<=(k-1) && pupil_Hausdorff <=(k-1)
                    hist_dist_v(k,i)=hist_dist_v(k,i)+1;
%                 else
%                     disp(files(j).name);
                end
            end
%         else
%               disp(files(j).name);
        end
    end
    progressbar(j/n);
end

hist_dist_v=hist_dist_v./n;

%c = [[0,1,0];[0,0,1];[1,0,0];[0.58,0,0.83]];       %���������m����ɫ��RGB�����
ls = {'k--','k-*','k-.','k-'};
lw = [1,1,1,1];
%c = rand(m,3);

for i=1:m
   % plot(0:1:15,hist_dist_v(:,i),'color',c(i,:),'LineWidth',2);
    plot(0:1:15,hist_dist_v(:,i),ls{i},'LineWidth',lw(i));
    hold on;
end
hold off;
xlabel('Hausdorff Distance','FontSize',20);
ylabel('Detection Rate','FontSize',20);
legend({'Method1','Method2','Method3','IrisParseNet'},'Interpreter','LaTex');
grid on;
% axis tight;
axis([0,15,0,1]);
hgsave(gcf,'F:\research\Iris\CASIA_hausdorff_comp.fig');
save('F:\research\Iris\CASIA_hausdorff.mat','hist_dist_v');
    
    
