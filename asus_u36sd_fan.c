/*
 *  asus_u36sd_fan.c - ASUS U36SD Fan Control driver using ACPI calls
 *
 *
 *  Copyright (C) 2013 Lukas Stancik
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  The development page for this driver is located at
 *
 *  Based upon asus_fan.c by Giulio Piemontese
 *  http://github.com/gpiemont/asusfan/
 *
 *  Previusly (by Dmitry Ursegov)
 *  http://code.google.com/p/asusfan/
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/acpi.h>

MODULE_AUTHOR("Lukas Stancik");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASUS U36SD Fan Control");

#ifndef ASUSFAN_VERBOSE
#define ASUSFAN_VERBOSE 0
#endif

#ifndef ASUSFAN_STABLE_RANGE
#define	ASUSFAN_STABLE_RANGE	1
#endif

#ifndef ASUSFAN_TEMP_NUM_SAMPLES
#define	ASUSFAN_TEMP_NUM_SAMPLES 5
#endif

MODULE_PARM_DESC(verbose, "Enable Speed changing/temperature status messages (0-2)");
int asusfan_verbose = ASUSFAN_VERBOSE;
module_param_named(verbose, asusfan_verbose, int, 0600);

MODULE_PARM_DESC(current_zone, "Current thermal zone");
int asusfan_curr_zone = -1;
module_param_named(current_zone, asusfan_curr_zone, int, 0444);

MODULE_PARM_DESC(previous_zone, "Previous thermal zone");
int asusfan_prev_zone = -1;
module_param_named(previous_zone, asusfan_prev_zone, int, 0444);

MODULE_PARM_DESC(current_speed, "Current fan speed");
int asusfan_curr_speed = -1;
module_param_named(current_speed, asusfan_curr_speed, int, 0444);

MODULE_PARM_DESC(target_speed, "Manually set target fan speed (Dangerous!)");
int asusfan_target_speed = -1;
module_param_named(target_speed, asusfan_target_speed, int, 0600);

MODULE_PARM_DESC(max_speed, "Maximum supported speed");
int asusfan_max_speed = 255;
module_param_named(max_speed, asusfan_max_speed, int, 0400);

MODULE_PARM_DESC(min_speed, "Minumum supported speed");
int asusfan_min_speed = 0;
module_param_named(min_speed, asusfan_min_speed, int, 0400);

MODULE_PARM_DESC(current_temp, "Current temperature value (°C)");
int asusfan_curr_temp = -1;
module_param_named(current_temp, asusfan_curr_temp, int, 0444);

MODULE_PARM_DESC(max_temp, "Maximum temperature value (°C) that can be evaluated");
int asusfan_max_temp = 110;
module_param_named(max_temp, asusfan_max_temp, int, 0444);

MODULE_PARM_DESC(min_temp, "Minimum temperature value (°C) that can be evaluated");
int asusfan_min_temp = 0;
module_param_named(min_temp, asusfan_min_temp, int, 0444);

MODULE_PARM_DESC(temp_stable_range, "Within this range (+/-) temperature is considered stable");
int asusfan_stable_range = ASUSFAN_STABLE_RANGE;
module_param_named(temp_stable_range, asusfan_stable_range, int, 0600);

MODULE_PARM_DESC(temp_num_samples, "Number of samples to establish the temperature behaviour");
int asusfan_num_samples = 0;
module_param_named(temp_num_samples, asusfan_num_samples, int, 0600);

/*
 * We took a pre-defined number of samples of temperature from the acpi,
 * then we try to guess the ongoing curve. This is done in a separated thread.
 */

unsigned int 	fan_sample = 0;

char 		fan_num_samples = ASUSFAN_TEMP_NUM_SAMPLES;
char		samples[ASUSFAN_TEMP_NUM_SAMPLES] ;

#define 	MONITOR_FREQ 1

/*
 * When the temperature goes down to a low zone it is better to stay at
 * a high speed some degrees more to reduce fan speed switching
 */

#define NUM_ZONES 	9
#define TMP_DIFF 	3

#define TIMER_FREQ 	10	/* seconds */

struct tmp_zone {
	int tmp;		/* °C */
	int speed;		/* 80-255 (0 - fan is disabled) */
	unsigned int sleep;	/* Sleep for sleep seconds more at tmp temperature */
};

int asusfan_current_zone = -1;

typedef enum status {
	stable,
	ascending,
	descending
} status_t;

status_t __thermal_status = stable;

const char * const status_name[3] = {
        "stable",
        "ascending",
        "descending"
};

MODULE_PARM_DESC(temp_status, "Current temperature status");
char * asusfan_temp_status = "";
module_param_named(temp_status, asusfan_temp_status, charp, 0444);

static struct tmp_zone zone[NUM_ZONES] = {  { 45,   0,1 },
                                            { 50,  80,1 },
                                            { 55, 100,1 },
                                            { 60, 110,1 },
                                            { 65, 130,1 },
                                            { 70, 140,2 },
                                            { 85, 160,3 },
                                            { 100,190,5 },
                                            { 105,210,7 }};

static void timer_handler(struct work_struct *work);
static void temp_status_timer(struct work_struct *work);

static DECLARE_DELAYED_WORK(ws, timer_handler);
static DECLARE_DELAYED_WORK(wst, temp_status_timer);

static struct workqueue_struct *wqs;
static struct workqueue_struct *wqst;

static int get_zone_temp(void)
{
        struct acpi_buffer output;
        union acpi_object out_obj;
        acpi_status status;

        int tmp;

        output.length = sizeof(out_obj);
        output.pointer = &out_obj;

        //Get current temperature
        status = acpi_evaluate_object(NULL, "\\_TZ.THRM._TMP",
                                        NULL, &output);
        if (status != AE_OK) printk("_TZ.THRM._TMP error\n");
        tmp = (int)((out_obj.integer.value-2732))/10;

	if((tmp > asusfan_max_temp ) || (tmp < asusfan_min_temp))
		return -1;

	if(asusfan_curr_temp != tmp)
		asusfan_curr_temp = tmp;

	return tmp;
}

static void set_fan_speed(int speed)
{
	struct acpi_object_list params;
	union acpi_object in_objs[2];
	acpi_status status;

	//Set fan speed
	params.count = ARRAY_SIZE(in_objs);
	params.pointer = &(*in_objs);
	in_objs[0].type = ACPI_TYPE_INTEGER;
	in_objs[0].integer.value = 1;
	in_objs[1].type = ACPI_TYPE_INTEGER;
	in_objs[1].integer.value = speed;
	if(asusfan_verbose)
		printk("ACPI called (_SB.PCI0.SBRG.EC0.SFNV 1 %d)\n", speed);
	status = acpi_evaluate_object(NULL, "\\_SB.PCI0.SBRG.EC0.SFNV",
					&params, NULL);
	if (status != AE_OK) printk("ASUS Fan ACPI call (_SB.PCI0.SBRG.EC0.SFNV 1 %d) error\n", speed);
}

static void timer_handler(struct work_struct *work)
{
	static int prev_zone = 0;
	int curr_zone = 0;
	int tmp;

	tmp =  get_zone_temp();

	if (unlikely(tmp == -1))
		return;

	if(tmp >= zone[NUM_ZONES-1].tmp)
		goto out;

	//Set fan speed and save previous zone
	for(curr_zone=0; curr_zone<(NUM_ZONES-1); curr_zone++)
		if(tmp < zone[curr_zone].tmp)
			break;

	if(unlikely(curr_zone < prev_zone &&
			tmp > zone[curr_zone].tmp - TMP_DIFF  )) {
		set_fan_speed(zone[prev_zone].speed);
		asusfan_curr_speed = zone[prev_zone].speed;
		if (asusfan_verbose > 0)
			printk("ASUS Fan: tmp = %d, curr zone %d, prev zone %d, speed set to %d\n",
						tmp, curr_zone, prev_zone, zone[prev_zone].speed);
		}
	else {
		set_fan_speed(zone[curr_zone].speed);
		asusfan_curr_speed = zone[curr_zone].speed;
		prev_zone = curr_zone;
		if (asusfan_verbose > 0)
			printk("asus_fan: tmp = %d, curr zone %d, prev zone %d, speed set to %d\n",
					tmp, curr_zone, prev_zone, zone[curr_zone].speed);
	}

out:

	queue_delayed_work(wqs, &ws, (TIMER_FREQ + zone[curr_zone].sleep) * HZ);
}

static void temp_status_timer(struct work_struct *work)
{
	int i = 0;
	int diff = 0;

	samples[0] = get_zone_temp();

	/* "Parabolic" tolerance curve */

	if ((fan_sample % ASUSFAN_TEMP_NUM_SAMPLES) == 0)
	{
		diff = (samples[0] - samples[fan_num_samples - 1]);

		if(asusfan_verbose > 1)
			printk("sample[0] = %d , sample[%d] = %d, diff = %d\n", samples[0], fan_num_samples - 1, samples[fan_num_samples-1], diff);

		if((diff <= asusfan_stable_range) && ( diff >= -asusfan_stable_range))
		{
			__thermal_status = stable;
			asusfan_temp_status = status_name[0];
		}
		else if(diff > asusfan_stable_range )
		{
			__thermal_status = ascending;
			asusfan_temp_status = status_name[1];
		}
		else if(diff < -asusfan_stable_range )
		{
			__thermal_status = descending;
			asusfan_temp_status = status_name[2];
		}
		if(asusfan_verbose > 1)
			printk("sample[0] = %d ", samples[0]);

		while(++i < fan_num_samples) {
			samples[fan_num_samples - i] = samples[fan_num_samples - i -1];
			if(asusfan_verbose > 1)
				printk("sample[%d] = %d ", i, samples[i]);
		}
		if(asusfan_verbose > 1)
			printk("\n");
	}

	queue_delayed_work(wqst, &wst, MONITOR_FREQ * HZ);
}

static int asus_fan_init(void)
{

	memset(&samples, 0, fan_num_samples);

	//Workqueue settings
	wqs = create_singlethread_workqueue("tmp");
	queue_delayed_work(wqs, &ws, HZ);

	wqst = create_singlethread_workqueue("sampler");
	queue_delayed_work(wqst, &wst, HZ);

	printk("ASUS U36SD Fan Control version 0.9\n");

	return 0;
}

static void asus_fan_exit(void)
{
    set_fan_speed(-1);

	cancel_delayed_work(&ws);
	flush_workqueue(wqs);
	destroy_workqueue(wqs);

	cancel_delayed_work(&wst);
	flush_workqueue(wqst);
	destroy_workqueue(wqst);

	printk("ASUS U36SD Fan Control driver unloaded\n");
}


module_init(asus_fan_init);
module_exit(asus_fan_exit);

