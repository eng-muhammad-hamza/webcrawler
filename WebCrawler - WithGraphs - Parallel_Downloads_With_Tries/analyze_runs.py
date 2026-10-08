import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from datetime import datetime
import os

def load_run_data():
    try:
        history = pd.DataFrame()    # Create an empty DataFrame to store history data
        if os.path.exists('metrics_history.csv') and os.path.getsize('metrics_history.csv') > 0:
            # Check if the CSV file exists and is not empty
            history = pd.read_csv('metrics_history.csv')                    # Read the CSV file into a DataFrame
            history['timestamp'] = pd.to_datetime(history['timestamp'])     # Convert the 'timestamp' column to datetime objects
        return history              # Return the DataFrame (empty if file doesn't exist or is empty)
    except Exception as e:
        print(f"Error loading data: {str(e)}")                              # Print any error that occurs during loading
        return None                                                         # Return None if there was an error

def create_trend_visualizations():
    # Load the data from previous runs
    data = load_run_data()
    # If no data or only one run, print a message and stop
    if data is None or len(data) < 1:
        print("Insufficient data for trend analysis (need at least 2 runs)")
        return

    # Create a new, large figure for all the plots
    plt.figure(figsize=(20, 16))
    # Add a big title at the top of the figure
    plt.suptitle("Web Crawler Performance Trends Across Runs", fontsize=20, y=0.98)
    # Set the background style for all plots to have a white grid
    sns.set_style("whitegrid")
    # Choose a set of 12 distinct colors for the plots
    palette = sns.color_palette("husl", 12)
    # Create a new column 'run' with formatted timestamps for labeling
    data['run'] = data['timestamp'].dt.strftime('%m/%d %H:%M')
    
    # 1. System Performance Trends
    # Create the first subplot (top left, spans two columns)
    ax1 = plt.subplot2grid((3, 3), (0, 0), colspan=4)
    # Draw a line showing total runtime for each run
    sns.lineplot(data=data, x='run', y='total_runtime', marker='o', 
                label='Total Runtime', color=palette[0], ax=ax1)
    # Draw a line showing time spent downloading URLs for each run
    sns.lineplot(data=data, x='run', y='url_download', marker='s', 
                label='URL Download', color=palette[1], ax=ax1)
    # Draw a line showing time spent processing (thread execution) for each run
    sns.lineplot(data=data, x='run', y='thread_execution', marker='D', 
                label='Processing', color=palette[2], ax=ax1)
    # Set the title and label for this plot
    ax1.set_title('System Performance Over Time', fontsize=14)
    ax1.set_ylabel('Time (ms)')
    # Show a legend to explain the lines
    ax1.legend()

    # 2. Throughput vs Efficiency
    # Create the fourth subplot (middle right, spans two columns)
    ax2 = plt.subplot2grid((3, 3), (1, 0), colspan=4)
    # Calculate average bytes processed per URL for each run
    data['bytes_per_url'] = data['total_bytes'] / data['urls_processed']
    # Calculate average time taken per URL for each run
    data['time_per_url'] = data['total_runtime'] / data['urls_processed']
    # Draw a scatter plot showing the relationship between bytes per URL and time per URL
    sns.scatterplot(data=data, x='bytes_per_url', y='time_per_url', 
                   hue='run', size='urls_processed', sizes=(50, 200),
                   palette=palette[4:7], ax=ax2)
    # Set the title and axis labels
    ax2.set_title('Throughput vs Efficiency', fontsize=14)
    ax2.set_xlabel('Bytes per URL')
    ax2.set_ylabel('ms per URL')
    # Move the legend outside the plot for clarity
    ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left')

    # 3. Worker Time Composition
    # Create the fifth subplot (bottom left)
    ax3 = plt.subplot2grid((3, 3), (2, 0), colspan=2)
    # List of columns for worker times (same as before)
    worker_time_cols = ['word_count_total', 'top_words_total', 'longest_word_total',
                      'sentence_count_total', 'char_freq_total', 'palindrome_total']
    # Calculate the average time spent by each worker task across all runs
    worker_time_data = data[worker_time_cols].mean()
    # Draw a pie chart showing the average share of time for each worker task
    worker_time_data.plot.pie(ax=ax3, autopct='%1.1f%%', startangle=90,
                            colors=palette[3:9])
    # Remove the y label (not needed for pie chart)
    ax3.set_ylabel('')
    # Set the title for this chart
    ax3.set_title('Average Worker Time Composition', fontsize=14)

    # 4. Performance Regression
    # Create the sixth subplot (bottom right, spans two columns)
    ax4 = plt.subplot2grid((3, 3), (2, 2), colspan=2)
    # Calculate the total time spent by all worker tasks for each run
    data['total_worker_time'] = data[worker_time_cols].sum(axis=1)
    # Draw a regression plot to see if total worker time predicts total runtime
    sns.regplot(data=data, x='total_worker_time', y='total_runtime', 
               scatter_kws={'s': 100, 'color': palette[10]}, 
               line_kws={'color': palette[11]}, ax=ax4)
    # Set the title and axis labels
    ax4.set_title('Worker Time vs Total Runtime', fontsize=14)
    ax4.set_xlabel('Total Worker Time (ms)')
    ax4.set_ylabel('Total Runtime (ms)')
    
    # For each run, add a label with the run's name to the regression plot
    for i, row in data.iterrows():
        ax4.text(row['total_worker_time'], row['total_runtime'], row['run'],
               fontsize=8, ha='center', va='bottom')

    # Adjust the layout so plots don't overlap
    plt.tight_layout()
    # Save the whole figure as an image file
    plt.savefig('comprehensive_trend_analysis.png', dpi=300, bbox_inches='tight')
    # Print a message to say the image was saved
    print("Trend analysis saved as 'comprehensive_trend_analysis.png'")
    # Show the figure on the screen
    plt.show()

if __name__ == "__main__":
    create_trend_visualizations()

