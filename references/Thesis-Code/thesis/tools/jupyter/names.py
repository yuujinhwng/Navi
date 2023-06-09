types = {
    'blur': 'Destructive',
    'noise': 'Additive',
    'combo': 'Combined'
}

type_map = {k: types[v] for k, v in {
    'bilateral_filter': 'blur',
    'gaussian_filter': 'blur',
    'mean_filter': 'blur',
    'median_filter': 'blur',
    'non_local_means': 'blur',
    'uniform_noise': 'noise',
    'gaussian_noise': 'noise',
    'cauchy_noise': 'noise',
    'laplacian_noise': 'noise',
    'snow': 'noise',
    'salt_and_pepper': 'noise',
    'super_filter': 'combo',
    'super_filter_reverse': 'combo'
}.items()}

filter_name_map = {
    'baseline': 'Baseline',
    'bilateral_filter': 'Bilateral filter',
    'gaussian_filter': 'Gaussian filter',
    'mean_filter': 'Mean filter',
    'median_filter': 'Median filter',
    'non_local_means': 'Non-local means',
    'uniform_noise': 'Uniform noise',
    'gaussian_noise': 'Gaussian noise',
    'cauchy_noise': 'Cauchy noise',
    'laplacian_noise': 'Laplacian noise',
    'snow': 'Snow noise',
    'salt_and_pepper': 'Salt-and-pepper noise',
    'super_filter': 'Comb',
    'super_filter_reverse': 'Comb Reverse'
}

pretty_name_map = {
    'gradient_entropy_iris_source': 'Entropy of source (iris) - gradient method',
    'gradient_entropy_iris_filtered': 'Entropy of result (iris) - gradient method',
    'gradient_mutual_information_iris': 'Mutual information (iris) - gradient method',
    'gabor_entropy_iris_source_1.0x': 'Entropy of source (iris) - gabor method (3px)',
    'gabor_entropy_iris_source_0.5x': 'Entropy of source (iris) - gabor method (6px)',
    'gabor_entropy_iris_source_0.25x': 'Entropy of source (iris) - gabor method (12px)',
    'gabor_entropy_iris_source_0.125x': 'Entropy of source (iris) - gabor method (24px)',
    'gabor_entropy_iris_source_0.0625x': 'Entropy of source (iris) - gabor method (48px)',
    'gabor_entropy_iris_filtered_1.0x': 'Entropy of result (iris) - gabor method (3px)',
    'gabor_entropy_iris_filtered_0.5x': 'Entropy of result (iris) - gabor method (6px)',
    'gabor_entropy_iris_filtered_0.25x': 'Entropy of result (iris) - gabor method (12px)',
    'gabor_entropy_iris_filtered_0.125x': 'Entropy of result (iris) - gabor method (24px)',
    'gabor_entropy_iris_filtered_0.0625x': 'Entropy of result (iris) - gabor method (48px)',
    'gabor_mutual_information_iris_1.0x': 'Mutual information (iris) - gabor method (3px)',
    'gabor_mutual_information_iris_0.5x': 'Mutual information (iris) - gabor method (6px)',
    'gabor_mutual_information_iris_0.25x': 'Mutual information (iris) - gabor method (12px)',
    'gabor_mutual_information_iris_0.125x': 'Mutual information (iris) - gabor method (24px)',
    'gabor_mutual_information_iris_0.0625x': 'Mutual information (iris) - gabor method (48px)',
    'gradient_entropy_image_source': 'Entropy of source (image) - gradient method',
    'gradient_entropy_image_filtered': 'Entropy of result (image) - gradient method',
    'gradient_mutual_information_image': 'Mutual information (image) - gradient method',
    'gabor_entropy_image_source_1.0x': 'Entropy of source (image) - gabor method (3px)',
    'gabor_entropy_image_source_0.5x': 'Entropy of source (image) - gabor method (6px)',
    'gabor_entropy_image_source_0.25x': 'Entropy of source (image) - gabor method (12px)',
    'gabor_entropy_image_source_0.125x': 'Entropy of source (image) - gabor method (24px)',
    'gabor_entropy_image_source_0.0625x': 'Entropy of source (image) - gabor method (48px)',
    'gabor_entropy_image_filtered_1.0x': 'Entropy of result (image) - gabor method (3px)',
    'gabor_entropy_image_filtered_0.5x': 'Entropy of result (image) - gabor method (6px)',
    'gabor_entropy_image_filtered_0.25x': 'Entropy of result (image) - gabor method (12px)',
    'gabor_entropy_image_filtered_0.125x': 'Entropy of result (image) - gabor method (24px)',
    'gabor_entropy_image_filtered_0.0625x': 'Entropy of result (image) - gabor method (48px)',
    'gabor_mutual_information_image_1.0x': 'Mutual information (image) - gabor method (3px)',
    'gabor_mutual_information_image_0.5x': 'Mutual information (image) - gabor method (6px)',
    'gabor_mutual_information_image_0.25x': 'Mutual information (image) - gabor method (12px)',
    'gabor_mutual_information_image_0.125x': 'Mutual information (image) - gabor method (24px)',
    'gabor_mutual_information_image_0.0625x': 'Mutual information (image) - gabor method (48px)',
    'iris_code_similarity': 'Iris code similarity',
    'image_normalized_similarity': 'Image similarity',
    'gaze_angle_error_source': 'Gaze error source',
    'gaze_angle_error_filtered': 'Gaze error result',
    'gaze_relative_error': 'Gaze error relative',
    'pupil_distance_else_pixel_error_source': 'Pupil pixel distance error of source - ELSE method',
    'pupil_distance_else_pixel_error_filtered': 'Pupil pixel distance error of result - ELSE method',
    'pupil_distance_deep_eye_pixel_error_source': 'Pupil pixel distance error of source - DeepEye method',
    'pupil_distance_deep_eye_pixel_error_filtered': 'Pupil pixel distance error of result - DeepEye method',
    'pupil_relative_error_else': 'Pupil pixel distance relative - ELSE method',
    'pupil_relative_error_deep_eye': 'Pupil pixel distance relative - DeepEye method',
    'filter': 'Filter',
    'k': 'Kernel size',
    'h': '$h$',
    'sigma': 'Variance',
    'scale': 'Variance',
    'intensity': 'Intensity',
    'density': 'Density',
    'sigma_s': 'Spatial variance',
    'sigma_c': 'Colour variance',

}