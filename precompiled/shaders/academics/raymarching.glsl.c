const float BIG = 1e20;
const float SMALL = 1e-20;

// "get_height_along_ray_over_world" gets the height at a point along the path
//   for a ray traveling over a world.
// NOTE: all input distances are relative to closest approach!
float get_height_along_ray_over_world(float x, float z2, float R){
    return sqrt(max(x*x + z2, 0.)) - R;
}
// "get_height_change_rate_along_ray_over_world" gets the rate at which height changes for a distance traveled along the path
//   for a ray traveling through the atmosphere.
// NOTE: all input distances are relative to closest approach!
float get_height_change_rate_along_ray_over_world(float x, float z2){
    return x / sqrt(max(x*x + z2, 0.));
}
// "get_air_density_ratio_at_height" gets the density ratio of an height within the atmosphere
// the "density ratio" is density expressed as a fraction of a surface value
float get_air_density_ratio_at_height(
    float h, 
    float H
){
    return exp(-h/H);
}
// "approx_air_column_density_ratio_along_ray_from_samples" returns an approximation 
//   for the columnar density ratio encountered by a ray traveling through the atmosphere.
// It is the integral of get_air_density_ratio_at_height() along the path of the ray, 
//   taking into account the height at every point along the path.
// We can't solve the integral in the usual fashion due to singularities
//   (see https://www.wolframalpha.com/input/?i=integrate+exp(-sqrt(x%5E2%2Bz%5E2)%2FH)+dx)
//   so we use a linear approximation for the height.
// Our linear approximation gets its slope and intercept from sampling
//   at points along the path (xm and xb respectively)
// NOTE: all input distances are relative to closest approach!
float approx_air_column_density_ratio_along_ray_from_samples(float x, float xm, float xb, float z2, float R, float H){
	float m = get_height_change_rate_along_ray_over_world(xm,z2);
	float b = get_height_along_ray_over_world(xb,z2,R);
	float h = m*(x-xb) + b;
    return -H/m * exp(-h/H);
}
// "approx_air_column_density_ratio_along_ray_for_segment" is a convenience wrapper for approx_air_column_density_ratio_along_ray_from_samples(), 
//   which calculates sensible values of xm and xb for the user 
//   given a specified range around which the approximation must be valid.
// The range is indicated by its lower bounds (xmin) and width (dx).
// NOTE: all input distances are relative to closest approach!
float approx_air_column_density_ratio_along_ray_for_segment(float x, float xmin, float dx, float z2, float R, float H){
    const float fm = 0.5;
    const float fb = 0.2;

    float xm   = xmin + fm*dx;
    float xb   = xmin + fb*dx;
    float xmax = xmin +    dx;

    return approx_air_column_density_ratio_along_ray_from_samples(clamp(x, xmin, xmax), xm, xb, z2,R,H);
}
// "approx_air_column_density_ratio_along_ray_for_absx" is a convenience wrapper for approx_air_column_density_ratio_along_ray_for_segment().
// It returns an approximation of columnar density ratio encountered from 
//   the surface of a world to a given upper bound, "x"
// Unlike approx_air_column_density_ratio_along_ray_from_samples() and approx_air_column_density_ratio_along_ray_for_segment(), 
//   it should be appropriate for any value of x, no matter if it's positive or negative.
// It does this by making two linear approximations for height:
//   one for the lower atmosphere, one for the upper atmosphere.
// These are represented by the two call outs to approx_air_column_density_ratio_along_ray_for_segment().
// "x" is the distance along the ray from closest approach to the upper bound (always positive),
//   or from the closest approach to the upper bound, if there is no intersection.
// "x_atmo" is the distance along the ray from closest approach to the top of the atmosphere (always positive)
// "x_world" is the distance along the ray from closest approach to the surface of the world (always positive)
// "sigma0" is the columnar density ratio generated by this equation when x is on the surface of the world;
//   it is used to express values for columnar density ratio relative to the surface of the world.
// "z2" is the closest distance from the ray to the center of the world, squared.
// NOTE: all input distances are relative to closest approach!
float approx_air_column_density_ratio_along_ray_for_absx(float x, float x_world, float x_atmo, float sigma0, float z2, float R, float H){
    // sanitize x_world so it's always positive
    x_world = abs(x_world);
    // sanitize x_atmo so it's always positive
    x_atmo  = abs(x_atmo);
    // sanitize x so it's always positive and greater than x_world
    x = max(abs(x)-x_world, 0.) + x_world;
    // "dx" is the width of the bounds covered by our linear approximations
    float dx = (x_atmo-x_world)/3.;

    return
        approx_air_column_density_ratio_along_ray_for_segment(x, x_world,    dx, z2,R,H)
      + approx_air_column_density_ratio_along_ray_for_segment(x, x_world+dx, dx, z2,R,H)
      - sigma0;
}
// "approx_reference_air_column_density_ratio_along_ray" is a convenience wrapper for approx_air_column_density_ratio_along_ray_for_absx().
// It returns a reference value that can be passed to approx_air_column_density_ratio_along_ray_2d().
// NOTE: all input distances are relative to closest approach!
float approx_reference_air_column_density_ratio_along_ray(float x_world, float x_atmo, float z2, float R, float H){
    return approx_air_column_density_ratio_along_ray_for_absx(x_world, x_world, x_atmo, 0., z2, R, H);
}
// "approx_air_column_density_ratio_along_ray_2d" is a convenience wrapper for approx_air_column_density_ratio_along_ray_for_absx().
// It returns a approximation of columnar density ratio that should be appropriate for any value of x.
// NOTE: all input distances are relative to closest approach!
float approx_air_column_density_ratio_along_ray_2d (float x_start, float x_stop, float x_world, float x_atmo, float sigma0, float z2, float R, float H){
    // NOTE: we clamp the result to prevent the generation of inifinities and nans, 
    // which can cause graphical artifacts.
    return 
        sign(x_stop)  * min(approx_air_column_density_ratio_along_ray_for_absx(x_stop,  x_world, x_atmo, sigma0, z2, R, H), BIG) -
    	sign(x_start) * min(approx_air_column_density_ratio_along_ray_for_absx(x_start, x_world, x_atmo, sigma0, z2, R, H), BIG); 
}
// "try_approx_air_column_density_ratio_along_ray" is an all-in-one convenience wrapper 
//   for approx_air_column_density_ratio_along_ray_2d() and approx_reference_air_column_density_ratio_along_ray.
// Just pass it the origin and direction of a 3d ray and it will find the column density ratio along its path, 
//   or return false to indicate the ray passes through the surface of the world.
float approx_air_column_density_ratio_along_line_segment (
	vec3  segment_origin, 
    vec3  segment_direction,
    float segment_length,
	vec3  world_position, 
	float world_radius, 
	float atmosphere_scale_height
){

    float z2;  					// distance ("radius") from the ray to the center of the world at closest approach, squared
    float x_z; 					// distance from the origin at which closest approach occurs

    float x_enter_atmo;  		// distance from the origin at which the ray enters the atmosphere
    float x_exit_atmo;   		// distance from the origin at which the ray exits the atmosphere

    float x_enter_world; 		// distance from the origin at which the ray strikes the surface of the world
    float x_exit_world;  		// distance from the origin at which the ray exits the world, assuming it could pass through

    // "atmosphere_radius" is the distance from the center of the world to the top of the atmosphere
    // NOTE: "12." is the number of scale heights needed to reach the official edge of space on Earth.
    // It should be sufficiently high to work for any world
    float atmosphere_radius = world_radius + 12. * atmosphere_scale_height;

    get_relation_between_ray_and_point(
		world_position, 
    	segment_origin,  segment_direction, 
		z2,			x_z 
	);
    try_get_relation_between_ray_and_sphere(
        world_radius,
        z2,            x_z,
        x_enter_world, x_exit_world 
    );

    bool is_obstructed = 
        0. < x_exit_world && x_exit_world < segment_length &&
        z2 < world_radius*world_radius;

    if (is_obstructed)
    {
    	return BIG;
    }

    try_get_relation_between_ray_and_sphere(
        atmosphere_radius,
        z2,            x_z, 
        x_enter_atmo,  x_exit_atmo
    );
    
    // NOTE: "sigma0" the column density ratio returned by approx_air_column_density_ratio_along_ray_for_absx() for the surface
    float sigma0 = approx_reference_air_column_density_ratio_along_ray(
    	x_exit_world-x_z, x_exit_atmo-x_z, 
    	z2, world_radius, atmosphere_scale_height
	);

    return approx_air_column_density_ratio_along_ray_2d( 
    	0.-x_z,           segment_length-x_z, 
    	x_exit_world-x_z, x_exit_atmo-x_z, sigma0, 
    	z2, world_radius, atmosphere_scale_height 
	);
}

vec3 get_rgb_intensity_of_light_rays_through_atmosphere(
    vec3  view_origin, vec3 view_direction,
    vec3  world_position, float world_radius,
    vec3  light_direction, vec3 light_rgb_intensity,
    vec3  background_rgb_intensity,
    float atmosphere_scale_height,
    vec3  beta_ray,
    vec3  beta_mie,
    vec3  beta_abs
){

    float unused1, unused2, unused3, unused4; // used for passing unused output parameters to functions

    const float VIEW_STEP_COUNT = 16.;// number of steps taken while marching along the view ray

    bool  view_is_scattered;  // whether view ray will enter the atmosphere
    bool  view_is_obstructed; // whether view ray will enter the surface of a world
    float view_z2;            // distance ("radius") from the view ray to the center of the world at closest approach, squared
    float view_x_z;           // distance along the view ray at which closest approach occurs
    float view_x_enter_atmo;  // distance along the view ray at which the ray enters the atmosphere
    float view_x_exit_atmo;   // distance along the view ray at which the ray exits the atmosphere
    float view_x_enter_world; // distance along the view ray at which the ray enters the surface of the world
    float view_x_exit_world;  // distance along the view ray at which the ray enters the surface of the world
    float view_x_start;       // distance along the view ray at which scattering starts, either because it's the start of the ray or the start of the atmosphere 
    float view_x_stop;        // distance along the view ray at which scattering no longer occurs, either due to hitting the world or leaving the atmosphere
    
    float view_dx;            // distance between steps while marching along the view ray
    float view_x;             // distance traversed while marching along the view ray
    float view_sigma;         // columnar density ratios for rayleigh and mie scattering, found by marching along the view ray. This expresses the quantity of air encountered along the view ray, relative to air density on the surface

    vec3  light_origin;       // absolute position while marching along the view ray
    float light_h;            // distance ("height") from the surface of the world while marching along the view ray
    float light_sigma;        // columnar density ratio encountered along the light ray. This expresses the quantity of air encountered along the light ray, relative to air density on the surface

    // "atmosphere_radius" is the distance from the center of the world to the top of the atmosphere
    //   We only set it to 3 scale heights because we are using this parameter for raymarching, and not a closed form solution
    float atmosphere_radius = world_radius + 12. * atmosphere_scale_height;

    // cosine of angle between view and light directions
    float cos_scatter_angle = dot(view_direction, light_direction); 

    // fraction of outgoing light transmitted across a given path
    vec3 fraction_outgoing = vec3(0);

    // fraction of incoming light transmitted across a given path
    vec3 fraction_incoming   = vec3(0);

    // total intensity for each color channel, found as the sum of light intensities for each path from the light source to the camera
    vec3  total_rgb_intensity = vec3(0); 

    // Rayleigh and Mie phase factors,
    // A.K.A "gamma" from Alan Zucconi: https://www.alanzucconi.com/2017/10/10/atmospheric-scattering-3/
    // This factor indicates the fraction of sunlight scattered to a given angle (indicated by its cosine, A.K.A. "cos_scatter_angle").
    // It only accounts for a portion of the sunlight that's lost during the scatter, which is irrespective of wavelength or density
    // The rest of the fractional loss is accounted for by the variable "betas", which is dependant on wavelength, 
    // and the density ratio, which is dependant on height
    // So all together, the fraction of sunlight that scatters to a given angle is: beta(wavelength) * gamma(angle) * density_ratio(height)
    float gamma_ray = get_rayleigh_phase_factor(cos_scatter_angle);
    float gamma_mie = get_henyey_greenstein_phase_factor(cos_scatter_angle);

    get_relation_between_ray_and_point(
        world_position, 
        view_origin,        view_direction, 
        view_z2,            view_x_z 
    );
    view_is_scattered   = try_get_relation_between_ray_and_sphere(
        atmosphere_radius,
        view_z2,            view_x_z, 
        view_x_enter_atmo,  view_x_exit_atmo
    );
    view_is_obstructed = try_get_relation_between_ray_and_sphere(
        world_radius,
        view_z2,            view_x_z,
        view_x_enter_world, view_x_exit_world 
    );

    // if view ray does not interact with the atmosphere
    // don't bother running the raymarch algorithm
    if (!view_is_scattered)
    {
        return background_rgb_intensity;
    }
    
    view_x_start = max(view_x_enter_atmo, 0.);
    view_x_stop  = view_is_obstructed? view_x_enter_world : view_x_exit_atmo;
    view_dx = (view_x_stop - view_x_start) / VIEW_STEP_COUNT;
    view_x  =  view_x_start + 0.5 * view_dx;

    for (float i = 0.; i < VIEW_STEP_COUNT; ++i)
    {
        light_origin = view_origin + view_direction * view_x;
        light_h      = get_height_along_ray_over_world(view_x-view_x_z, view_z2, world_radius);

        view_sigma  = approx_air_column_density_ratio_along_line_segment (
            view_origin,    view_direction,  view_x,
            world_position, world_radius, atmosphere_scale_height
        );

        light_sigma  = approx_air_column_density_ratio_along_line_segment (
            light_origin,   light_direction, 3.*world_radius,
            world_position, world_radius, atmosphere_scale_height
        );

        total_rgb_intensity += light_rgb_intensity
            // outgoing fraction: the fraction of light that scatters away from camera
            * exp(-(beta_ray + beta_mie + beta_abs) * (view_sigma + light_sigma))
            // incoming fraction: the fraction of light that scatters towards camera
            * view_dx * exp(-light_h/atmosphere_scale_height) * (beta_ray * gamma_ray + beta_mie * gamma_mie);

        view_x += view_dx;
    }

    // now calculate the intensity of light that traveled straight in from the background, and add it to the total
    total_rgb_intensity += background_rgb_intensity 
        // outgoing fraction: the fraction of light that would travel straight towards camera, but gets diverted
        * exp(-(beta_ray + beta_mie + beta_abs) * view_sigma);

    return total_rgb_intensity;
}